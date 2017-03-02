/*This file is part of the Maslow Control Software.

    The Maslow Control Software is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Maslow Control Software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Maslow Control Software.  If not, see <http://www.gnu.org/licenses/>.
    
    Copyright 2014-2017 Bar Smith*/ 


#include "Arduino.h"
#include "GearboxMotorEncoder.h"

GearboxMotorEncoder::GearboxMotorEncoder(int pwmPin, int directionPin1, int directionPin2, int encoderPin1, int encoderPin2, int eepromAdr)
:
_encoder(encoderPin1,encoderPin2)
{
    
    //initialize motor
    _motor.setupMotor(pwmPin, directionPin1, directionPin2);
    
    
    //initialize variables
    _eepromAdr    = eepromAdr;
    
    //read the motor calibration curve from memory
    _readAllLinSegs(_eepromAdr);
}

//           Encoder Functions

long         GearboxMotorEncoder::readEncoder(){
    return _encoder.read();
}

void         GearboxMotorEncoder::writeEncoder(long newEncoderValue){
    _encoder.write(newEncoderValue);
}


//            Address motor module functions

void         GearboxMotorEncoder::write(int speed){
    //_motor.write(speed);
}

void         GearboxMotorEncoder::attach(){
    _motor.attach();
}

void         GearboxMotorEncoder::detach(){
    if (_motor.attached()){
        EEPROM.write(_eepromAdr, EEPROMVALIDDATA);      //Mark that there is data to be written
        //_writeFloat (_eepromAdr+SIZEOFFLOAT, read());   //Write the axis position (this should happen at the axis level)
        
        _writeAllLinSegs(_eepromAdr);
        
    }
    
    _motor.detach();
}

void         GearboxMotorEncoder::computePID(){
                static float avg1; //this is a gross way to do this and should be changed
                static float avg2;
                static float avg3;
                static float avg4;
                static float avg5;
                static float avg6;
                static float avg7;
                
                float tempSpeed = _speedSinceLastCall();
                float avgSpeed  = (avg1 + avg2 + avg3 + avg4 + avg5 + avg6 + avg7 + tempSpeed)/8; //the speed since last call function is somewhat noisy because calculating the motor speed 100x/second is pushing the limits on the encoder resolution
                
                float errorTerm = avgSpeed - _speedSetpoint;
                int pwmCmd = _kP*errorTerm;//should be in the range 0-255
                _motor.write(pwmCmd);
                Serial.println(avgSpeed);
                //Serial.print(" ");
                //Serial.println(_speedSetpoint);
                //Serial.print(" ");
                //Serial.println(pwmCmd);
                
                avg1 = avg2;
                avg2 = avg3;
                avg3 = avg4;
                avg4 = avg5;
                avg5 = avg6;
                avg6 = avg7;
                avg7 = tempSpeed;
}

//           Reading and Writing EEPROM

float        GearboxMotorEncoder::_readFloat(unsigned int addr){

//readFloat and writeFloat functions courtesy of http://www.alexenglish.info/2014/05/saving-floats-longs-ints-eeprom-arduino-using-unions/


    union{
        byte b[4];
        float f;
    } data;
    for(int i = 0; i < 4; i++)
    {
        data.b[i] = EEPROM.read(addr+i);
    }
    return data.f;
}

void         GearboxMotorEncoder::_writeFloat(unsigned int addr, float x){
    
    //Writes a floating point number into the eeprom memory by splitting it into four one byte chunks and saving them
    
    union{
        byte b[4];
        float f;
    } data;
    data.f = x;
    for(int i = 0; i < 4; i++){
        EEPROM.write(addr+i, data.b[i]);
    }
}

void         GearboxMotorEncoder::_writeLinSeg(unsigned int addr, LinSegment linSeg){
    
    //flag that data is good
    EEPROM.write(addr, EEPROMVALIDDATA);
    
    _writeFloat(addr + 1                , linSeg.slope);
    _writeFloat(addr + 1 + 1*SIZEOFFLOAT, linSeg.intercept);
    _writeFloat(addr + 1 + 2*SIZEOFFLOAT, linSeg.positiveBound);
    _writeFloat(addr + 1 + 3*SIZEOFFLOAT, linSeg.negativeBound);
}

void         GearboxMotorEncoder::_writeAllLinSegs(unsigned int addr){
    /*
    Write all of the LinSegment objects which define the motors response into the EEPROM
    */
    
    addr = addr + 2*SIZEOFFLOAT;
    
    //Write into memory
    _writeLinSeg(addr                 , _motor.getSegment(0));
    _writeLinSeg(addr + 1*SIZEOFLINSEG, _motor.getSegment(1));
    _writeLinSeg(addr + 2*SIZEOFLINSEG, _motor.getSegment(2));
    _writeLinSeg(addr + 3*SIZEOFLINSEG, _motor.getSegment(3));
    
}

LinSegment   GearboxMotorEncoder::_readLinSeg(unsigned int addr){
    /*
    Read a LinSegment object from the EEPROM
    */
    
    LinSegment linSeg;
    
    //check that data is good
    if (EEPROM.read(addr) == EEPROMVALIDDATA){
        linSeg.slope         =  _readFloat(addr + 1                );
        linSeg.intercept     =  _readFloat(addr + 1 + 1*SIZEOFFLOAT);
        linSeg.positiveBound =  _readFloat(addr + 1 + 2*SIZEOFFLOAT);
        linSeg.negativeBound =  _readFloat(addr + 1 + 3*SIZEOFFLOAT);
    }
    
    return linSeg;
}

void         GearboxMotorEncoder::_readAllLinSegs(unsigned int addr){
   /*
   Read back all the LinSegment objects which define the motors behavior from the EEPROM
   */
    
    addr = addr + 8;
    
    LinSegment linSeg;
    
    for (int i = 0; i < 4; i++){
        linSeg = _readLinSeg (addr + i*SIZEOFLINSEG);
        
        _motor.setSegment(i, linSeg.slope, linSeg.intercept,  linSeg.negativeBound, linSeg.positiveBound);
    }
}


//           Calibration functions

void         GearboxMotorEncoder::computeMotorResponse(){
    
    //remove whatever transform is applied
    _motor.setSegment(0 , 1, 0, -255, 255);
    _motor.setSegment(1 , 1, 0, 0, 0);
    _motor.setSegment(2 , 1, 0, 0, 0);
    _motor.setSegment(3 , 1, 0, 0, 0);
    
    //In the positive direction
    //-----------------------------------------------------------------------------------
    
    float scale = 255/measureMotorSpeed(255); //Y3*scale = 255 -> scale = 255/Y3
    
    int stallPoint;
    float motorSpeed;
    
    int upperBound = 255; //the whole range is valid
    int lowerBound =   0;
    
    while (true){ //until a value is found
        Serial.print("Testing: ");
        Serial.println((upperBound + lowerBound)/2);
        
        motorSpeed = measureMotorSpeed((upperBound + lowerBound)/2);
        
        if (motorSpeed == 0){                               //if the motor stalled
            lowerBound = (upperBound + lowerBound)/2;           //shift lower bound to be the guess
            Serial.println("- stall");
        }
        else{                                               //if the motor didn't stall
            upperBound = (upperBound + lowerBound)/2;           //shift upper bound to be the guess
            Serial.println("- good");
        }
        
        if (upperBound - lowerBound <= 1){                  //when we've converged on the first point which doesn't stall
            break;                                              //exit loop
        }
    }
    
    stallPoint = upperBound;
    
    Serial.print("decided on final value of: ");
    Serial.println(stallPoint);
    
    
    //compute a model of the motor from the given data points
    float X1 = stallPoint;
    float Y1 = scale*motorSpeed;
    
    float X2 = (255 - X1)/2;
    float Y2 = scale*measureMotorSpeed(X2);
    
    float X3 = 255;
    float Y3 = 255;
    
    float M1 = (Y1 - Y2)/(X1 - X2);
    float M2 = (Y2 - Y3)/(X2 - X3);
    
    float I1 = -1*(Y1 - (M1*X1));
    float I2 = -1*(Y2 - (M2*X2));
    
    
    //Apply the model to the motor
    _motor.setSegment(2 , M1, I1,    0,   Y2);
    _motor.setSegment(3 , M2, I2, Y2-1, Y3+1);
    
    
    //In the negative direction
    //-----------------------------------------------------------------------------
    
    scale = -255/measureMotorSpeed(-255); //Y3*scale = 255 -> scale = 255/Y3
    
    upperBound =      0; //the whole range is valid
    lowerBound =   -255;
    
    while (true){ //until a value is found
        
        Serial.print("Testing: ");
        Serial.println((upperBound + lowerBound)/2);
        
        motorSpeed = measureMotorSpeed((upperBound + lowerBound)/2);
        if (motorSpeed == 0){                               //if the motor stalled
            upperBound = (upperBound + lowerBound)/2;           //shift lower bound to be the guess
            Serial.println("-stall");
        }
        else{                                               //if the motor didn't stall
            lowerBound = (upperBound + lowerBound)/2;           //shift upper bound to be the guess
            Serial.println("-good");
        }
        
        if (upperBound - lowerBound <= 1){                  //when we've converged on the first point which doesn't stall
            break;                                              //exit loop
        }
    }
    
    stallPoint = lowerBound;
    
    Serial.print("decided on a final value of: ");
    Serial.println(stallPoint);
    
    //At this point motorSpeed is the speed in RPM at the value i which is just above the stall speed
    
    
    //Compute a model for the motor's behavior using the given data-points
    X1 = stallPoint;
    Y1 = scale*motorSpeed;
    
    X2 = (-255 - X1)/3 + X1;
    Y2 = scale*measureMotorSpeed(X2);
    
    X3 = -255;
    Y3 = -255;
    
    M1 = (Y1 - Y2)/(X1 - X2);
    M2 = (Y2 - Y3)/(X2 - X3);
    
    I1 = -1*(Y1 - (M1*X1));
    I2 = -1*(Y2 - (M2*X2));
    
    
    //Apply the model to the motor
    _motor.setSegment(0 , M1, I1,   Y2,    0);
    _motor.setSegment(1 , M2, I2, Y3-1, Y2+1);
    
    Serial.println("Calibration complete.");
    
}

float        GearboxMotorEncoder::_speedSinceLastCall(){
    /*
    Return the average motor speed since the last time the function was called in units of RPM
    */
    
    //static variables to persist between calls
    static long time = micros();
    static long prevEncoderValue = _encoder.read();
    
    //compute dist moved
    long elapsedTime = micros() - time;                     //units of microseconds 1/60,000,000 of a minute
    int distMoved   = _encoder.read() - prevEncoderValue;   //units of steps moved of which there are NUMBER_OF_ENCODER_STEPS per rotation
    int conversionFactor = 7364;//60 million / number of steps per rotation
    float RPM = ((float)conversionFactor*(float)distMoved)/(float)elapsedTime;
    
    //set values for next call
    time = micros();
    prevEncoderValue = _encoder.read();
    
    //return the speed in RPM
    return RPM;
}

float        GearboxMotorEncoder::measureMotorSpeed(int speed){
    /*
    Returns the motors speed in RPM at a given input value
    */
    
    int sign = speed/abs(speed);
    
    _disableAxisForTesting = true;
    attach();
    
    int numberOfStepsToTest = 2000;
    int timeOutMS           = 30*1000; //30 seconds
    bool stall              = false;
    
    //run the motor for numberOfStepsToTest steps positive and record the time taken
    
    
    long originalEncoderPos  = _encoder.read();
    long startTime = millis();
    
    //until the motor has moved the target distance
    while (abs(originalEncoderPos - _encoder.read()) < numberOfStepsToTest){
        //establish baseline for speed measurement
        _speedSinceLastCall();
        
        //command motor to spin at speed
        _motor.write(speed);
        
        //wait
        delay(200);
        
        //print to prevent connection timeout
        Serial.println("pt(0, 0, 0)mm");
        
        //check to see if motor is moving
        if (_speedSinceLastCall() < .01){
            stall = true;
            break;
        }
    }
    int posTime = millis() - startTime;
    
    //rotations = number of steps taken / steps per rotation
    float rotations = (originalEncoderPos - _encoder.read())/NUMBER_OF_ENCODER_STEPS;
    //minutes = time elapsed in ms * 1000ms/s *60 seconds per minute
    float minutes   = posTime/(1000.0*60.0);
    
    //RPM is rotations per minute.
    float RPM = rotations/minutes;
    
    if (stall){
        RPM = 0;
    }
    
    //move back to start point
    _disableAxisForTesting = false;
    for (long startTime = millis(); millis() - startTime < 2000; millis()){
        _motor.write(_encoder.read() - originalEncoderPos);
        delay(50);
        //print to prevent connection timeout
        Serial.println("pt(0, 0, 0)mm");
    }
    
    return RPM;
}

void         GearboxMotorEncoder::testPID(int speed){
                delay(500);
                Serial.println("Test PID");
                _motor.attach();
                _speedSetpoint = 8.0;
                int i = 0;
                while(true){
                    delay(10);
                    if(i > 1000){
                        _speedSetpoint = 14;
                    }
                    if(i > 2000){
                        _speedSetpoint = -8;
                        i = 0;
                    }
                    i++;
                }
}