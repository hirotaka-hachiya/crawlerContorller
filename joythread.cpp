#include "joythread.h"

// constructor for setting joystick id (JOYSTICKID1) and initializing JOYINFOEX
JoyThread::JoyThread()
{
    runFlag = true;    // running flag ON

    // initialize JOYINFOEX
    joyInfo.dwSize = sizeof JOYINFOEX;
    joyInfo.dwFlags = JOY_RETURNALL;
}

void JoyThread::startJoyThread(int joyId, QStringList leftMotorList, QStringList rightMotorList, QStringList indi1MotorList, QStringList indi2MotorList, int numMotors)
{
    qDebug()<<"startJoyThread: "<<currentThreadId();

    // set joystick id and motor ids
    this->joyId = joyId;

    // set motor ids
    this->numMotors = numMotors;
    this->leftMotorList = leftMotorList;
    this->rightMotorList = rightMotorList;
    this->indi1MotorList = indi1MotorList;
    this->indi2MotorList = indi2MotorList;

    // stop thread if thread is already running
    if(isRunning())
        stop();

    // start thread
    start();
    runFlag = true;
}

void JoyThread::startServerJoyThread(int joyId, QTcpSocket *mySocket, QStringList leftMotorList, QStringList rightMotorList, QStringList indi1MotorList, QStringList indi2MotorList, int numMotors)
{
    qDebug()<<"startSocketJoyThread: "<<currentThreadId();

    // set IP address, TCP port and motor ids
    this->mySocket = mySocket;

    // set connection mode
    connectMode = SERVER_MODE;

    // start thread
    startJoyThread(joyId, leftMotorList, rightMotorList, indi1MotorList, indi2MotorList, numMotors);
}

void JoyThread::startDeviceJoyThread(int joyId, RoboteqDevice *myDevice, QStringList leftMotorList, QStringList rightMotorList, QStringList indi1MotorList, QStringList indi2MotorList, int numMotors)
{
    qDebug()<<"startDeviceJoyThread: "<<currentThreadId();

    // set roboteQ device
    this->myDevice = myDevice;

    // set connection mode
    connectMode = DEVICE_MODE;

    // start thread
    startJoyThread(joyId, leftMotorList, rightMotorList, indi1MotorList, indi2MotorList, numMotors);
}

// method for getting joystick position and sending to UI
void JoyThread::run()
{

    //---------------------------
    // test if each motor is available (alive)
    aliveMotorList.clear();
    for(int motorID=1;motorID<=numMotors;motorID++){

        // move motor
        SetCommand("_VAR",motorID,500,connectMode);

        // mesure amp value
        double sumAmp = 0;
        for(int ampTrial=0;ampTrial<10;ampTrial++)
            sumAmp = sumAmp + GetValue("_VAR",motorID+10,connectMode).toDouble();

        // stop motor
        SetCommand("_VAR",motorID,0,connectMode);

        // add motor id if average amp is higher than threshold
        if(sumAmp/10 > 1)
            aliveMotorList.append(QString::number(motorID));

        qDebug() << "average Amp=" << sumAmp/10;
    }

    setAliveMotors(aliveMotorList);
    //---------------------------

    while(runFlag) // loop if runFlag on
    {
        // get joystick position
        if (joyGetPosEx(joyId, &joyInfo) == JOYERR_NOERROR){

            //convert to QString
            QString x  = QString::number(joyInfo.dwXpos);
            QString y  = QString::number(joyInfo.dwYpos);
            QString z  = QString::number(joyInfo.dwZpos);
            QString r  = QString::number(joyInfo.dwRpos);
            QString bt = QString::number(joyInfo.dwButtons);

            // emit joystick value
            emit printJoyValue(x,y,z,r,bt);

            //----------------------
            // convert to motor value
            double throttleValue = ((double)joyInfo.dwYpos - joyMidValue)/joyMidValue*motorMaxValue;
            double steeringRatio = ((double)joyInfo.dwXpos - joyMidValue)/joyMidValue;
            double indiThrottleValue1 = ((double)joyInfo.dwRpos - joyMidValue)/joyMidValue*motorMaxValue;
            double indiThrottleValue2 = ((double)joyInfo.dwZpos - joyMidValue)/joyMidValue*motorMaxValue;
            double leftMotorValue = 0;
            double rightMotorValue = 0;

            // rotation at the same position
            if(abs(throttleValue) < throttleMaxRotValue){
                leftMotorValue  = limitValue(-steeringRatio*motorMaxValue,motorMinValue,motorMaxValue);
                rightMotorValue = limitValue( steeringRatio*motorMaxValue,motorMinValue,motorMaxValue);

            // move
            }else{
                leftMotorValue  = limitValue((1 + steeringRatio)*throttleValue,motorMinValue,motorMaxValue);
                rightMotorValue = limitValue((1 - steeringRatio)*throttleValue,motorMinValue,motorMaxValue);
            }
            qDebug("throttleValue=%f steeringRatio=%f leftMotor=%d rightMotor=%d indiThrottleValue1=%d indiThrottleValue2=%d",throttleValue,steeringRatio,(int)leftMotorValue,(int)rightMotorValue,(int)indiThrottleValue1, (int)indiThrottleValue2);
            //----------------------

            //----------------------
            // send command to server 
            QString tmpStr;
            QByteArray sendMsg;
            QByteArray recvMsg;

            //----------
            // set velocity value


            //if(!abs(((double)joyInfo.dwYpos - joyMidValue)) && !abs(((double)joyInfo.dwXpos - joyMidValue))){
            if(abs(((double)joyInfo.dwRpos - joyMidValue)) || abs(((double)joyInfo.dwZpos - joyMidValue))){
            //if(abs(indiThrottleValue1) > throttleMaxRotValue || abs(indiThrottleValue2) > throttleMaxRotValue){
            //if(joyInfo.dwButtons&JOY_BUTTON7){
                // individual motors group 1
                for(QString id: indi1MotorList)
                    SetCommand("_VAR",id.toInt(),(int)indiThrottleValue1,connectMode);

                // individual motors group 2
                for(QString id: indi2MotorList)
                    SetCommand("_VAR",id.toInt(),(int)indiThrottleValue2,connectMode);

            }else{
                // left motors, inverse value
                for(QString id: leftMotorList)
                    SetCommand("_VAR",id.toInt(),-leftMotorValue,connectMode);

                // right motors
                for(QString id: rightMotorList)
                    SetCommand("_VAR",id.toInt(),rightMotorValue,connectMode);
            }
            //----------

            //----------
            // get motor power
            QStringList pow;

            for(int i=1;i<=numMotors;i++)
                pow.append(GetValue("_VAR",i+10,connectMode));

            // emit motor power signals
            emit printPowerValue(pow);
            //----------

            /*
            //----------
            // get encoder value
            // motor 1
            QStringList enc;
            enc.append(GetValueSocket("_C",1));
            enc.append(GetValueSocket("_C",2));

            // emit encoder signals
            emit printEncoderValue(enc);
            //----------
            */

            Sleep(100);
            //----------------------
        }
    }

    //----------------------
    // loops is over and then disconnect
    if(connectMode == SERVER_MODE){
        // close socket connection
        mySocket->write("_DIS ");

        // wait for writing command data and receiving feedback from server
        mySocket->waitForBytesWritten();
    }else if(connectMode == DEVICE_MODE){
        // close device connection
        myDevice->Disconnect();
    }
    //----------------------
}

// send specified command with id and value to the server
QByteArray JoyThread::SetCommandDevice(QString cmd, quint8 id, int val)
{

    // send command to RoboteQ driver
    int status = 0;
    if(!cmd.compare("_VAR")){
        if((status = myDevice->SetCommand(_VAR, id, val)) != RQ_SUCCESS)
            qDebug() << "failed --> "<<status;
        else
            qDebug() << "succeeded.";
    }

    // convert "status" integer to QByteArray
    QByteArray recvMsg;
    recvMsg.append(QString::number(status));

    return recvMsg;
}

// send specified command with id and value to the server
QByteArray JoyThread::GetValueDevice(QString cmd, quint8 id)
{
    int recvMsgInt=0;
    int status = 0;

    // receive feedback from RoboteQ driver
    if(!cmd.compare("_VAR")){
        if((status = myDevice->GetValue(_VAR, id, recvMsgInt)) != RQ_SUCCESS)
            qDebug()<<"GetValue failed --> "<<status;
        else
            qDebug()<<"GetValue succeeded.";
    }

    // convert "status"recvMsgInt" integer to QByteArray
    QByteArray recvMsg;
    recvMsg.append(QString::number(recvMsgInt));
    qDebug() << "recvMsg" << recvMsg;

    return recvMsg;
}

// send specified command with id and value to the server
QByteArray JoyThread::SetCommandSocket(QString cmd, quint8 id, int val)
{
    // make command
    QByteArray sendMsg = QString("%1 %2 %3").arg(cmd).arg(id).arg(val).toUtf8();

    // send command to server through socket "mySocket"
    mySocket->write(sendMsg);

    // wait for writing command data and receiving feedback from server
    mySocket->waitForBytesWritten();
    mySocket->waitForReadyRead();

    // receive feedback from server
    QByteArray recvMsg = mySocket->read(256);

    /*
    // print out messages
    qDebug(sendMsg);
    qDebug(recvMsg);
    */

    return recvMsg;
}

// send specified command with id and value to the server
QByteArray JoyThread::GetValueSocket(QString cmd, quint8 id)
{
    // make command
    QByteArray sendMsg = QString("%1 %2").arg(cmd).arg(id).toUtf8();

    // send command to server through socket "mySocket"
    mySocket->write(sendMsg);

    // wait for writing command data and receiving feedback from server
    mySocket->waitForBytesWritten();
    mySocket->waitForReadyRead();

    // receive feedback from server
    QByteArray recvMsg = mySocket->read(256);

    /*
    // print out messages
    qDebug(sendMsg);
    qDebug(recvMsg);
    */

    return recvMsg;
}

// send specified command with id and value to the server or driver
QByteArray JoyThread::SetCommand(QString cmd, quint8 id, int val, int mode)
{
    QByteArray recvMsg;

    switch(mode){
    case SERVER_MODE:
        recvMsg = SetCommandSocket(cmd,id,val);

    case DEVICE_MODE:
        recvMsg = SetCommandDevice(cmd,id,val);
    }

    return recvMsg;
}

// send specified command with id and value to the server or driver
QByteArray JoyThread::GetValue(QString cmd, quint8 id, int mode)
{
    QByteArray recvMsg;

    switch(mode){
    case SERVER_MODE:
        recvMsg = GetValueSocket(cmd,id);

    case DEVICE_MODE:
        recvMsg = GetValueDevice(cmd,id);
    }

    return recvMsg;
}


// method for stopping thred
void JoyThread::stop()
{
    runFlag = false; // running flag OFF
    wait();
}

// method for changing invidual controlled motor id
void JoyThread::changeMotorIDs(QStringList leftMotorList, QStringList rightMotorList, QStringList indi1MotorList, QStringList indi2MotorList)
{
    this->leftMotorList = leftMotorList;
    this->rightMotorList = rightMotorList;
    this->indi1MotorList = indi1MotorList;
    this->indi2MotorList = indi2MotorList;
}

// destructor
JoyThread::~JoyThread()
{
    runFlag = false;
    wait();
}

// limit value in then range between min and max
double JoyThread::limitValue(double value,double min,double max)
{
    value = (value < min) ? min : value;
    value = (value > max) ? max : value;
    return value;
}
