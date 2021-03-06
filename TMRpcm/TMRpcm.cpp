/*Library by TMRh20 2012-2013*/

#include <SD.h>
#include <TMRpcm.h>

const int buffSize = 100; //note: there are 2 sound buffers. This will require (soundBuff*4) memory free
volatile unsigned int buffer[2][buffSize+1], buffCount = 0, resolution = 500;
volatile boolean buffEmpty[2] = {false,false}, whichBuff = false, loadCounter=0, playing = 0;
unsigned int tt=0;
int volMod=0;
boolean paused = 0;

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega1281__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega2561__)
	volatile byte *TIMSK[] = {&TIMSK1,&TIMSK3,&TIMSK4,&TIMSK5};
	volatile byte *TCCRnA[] = {&TCCR1A,&TCCR3A,&TCCR4A,&TCCR5A};
	volatile byte *TCCRnB[] = {&TCCR1B, &TCCR3B,&TCCR4B,&TCCR5B};
	volatile unsigned int *OCRnA[] = {&OCR1A,&OCR3A,&OCR4A,&OCR5A};
	volatile unsigned int *OCRnB[] = {&OCR1B, &OCR3B,&OCR4B,&OCR5B};
	volatile unsigned int *ICRn[]	= {&ICR1, &ICR3,&ICR4,&ICR5};

	ISR_ALIAS(TIMER3_OVF_vect, TIMER1_OVF_vect);
	ISR_ALIAS(TIMER3_CAPT_vect, TIMER1_CAPT_vect);
	ISR_ALIAS(TIMER4_OVF_vect, TIMER1_OVF_vect);
	ISR_ALIAS(TIMER4_CAPT_vect, TIMER1_CAPT_vect);
	ISR_ALIAS(TIMER5_OVF_vect, TIMER1_OVF_vect);
	ISR_ALIAS(TIMER5_CAPT_vect, TIMER1_CAPT_vect);
#else
	volatile byte *TIMSK[] = {&TIMSK1};
	volatile byte *TCCRnA[] = {&TCCR1A};
	volatile byte *TCCRnB[] = {&TCCR1B};
	volatile unsigned int *OCRnA[] = {&OCR1A};
	volatile unsigned int *OCRnB[] = {&OCR1B};
	volatile unsigned int *ICRn[]	= {&ICR1};
#endif


File sFile;


void TMRpcm::setPin(){

	disable();
	pinMode(speakerPin,OUTPUT);
	switch(speakerPin){
		case 5: tt=1; break; //use TIMER3
		case 6: tt=2; break;//use TIMER4
		case 46:tt=3; break;//use TIMER5
		default:tt=0; break; //useTIMER1 as default
	}
}


void TMRpcm::play(char* filename){

  if(speakerPin != lastSpeakPin){ setPin(); lastSpeakPin=speakerPin;}
  stopPlayback();

  if(sFile){sFile.close();}
  if(!wavInfo(filename) ){ return; }//verify its a valid wav file
  sFile = SD.open(filename);

  if(sFile){
	playing = 1; paused = 0;
    sFile.seek(44); //skip the header info

	if(SAMPLE_RATE > 22050 ){ SAMPLE_RATE = 18000; Serial.print("SampleRate Too High");}

    resolution = 8 * (1000000/SAMPLE_RATE);

    for(int i=0; i<buffSize; i++){ buffer[0][i] = i,buffSize; }
    for(int i=0; i<buffSize; i++){ buffer[1][i] = i+buffSize;  }
    whichBuff = 0; buffEmpty[0] = 0; buffEmpty[1] = 0; buffCount = 0;

    noInterrupts();
    *ICRn[tt] = resolution;
    *OCRnA[tt] = *OCRnB[tt] = 1;
//    if(pwmMode){
      *TCCRnA[tt] = _BV(WGM11) | _BV(COM1A1) | _BV(COM1B0) | _BV(COM1B1); //WGM11,12,13 all set to 1 = fast PWM/w ICR TOP
      *TCCRnB[tt] = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
//    }
//    else{
//      *TCCRnA[tt] = _BV(COM1A1) | _BV(COM1B0) | _BV(COM1B1);
//      *TCCRnB[tt] = _BV(WGM13) | _BV(CS10);
//    }

   *TIMSK[tt] = ( _BV(ICIE1) | _BV(TOIE1) );
    interrupts();


  }else{Serial.println("Read fail"); }

}



void TMRpcm::pause(){
	paused = !paused;
}


void TMRpcm::volume(int upDown){

  if(upDown){
	  volMod++; volMod = min(volMod,3);
  }else{
	  volMod--; volMod = max(volMod, -4);
  }

}


boolean TMRpcm::wavInfo(char* filename){

  //check for the string WAVE starting at 8th bit in header file
  File xFile = SD.open(filename);
  if(!xFile){return 0;}
  xFile.seek(8);
  char wavStr[] = {'W','A','V','E'};
  for (int i =0; i<4; i++){
	  if(xFile.read() != wavStr[i]){ Serial.println("WAV File Error"); break; }
  }

    xFile.seek(24);
    SAMPLE_RATE = xFile.read();
    SAMPLE_RATE = xFile.read() << 8 | SAMPLE_RATE;

    //verify that Bits Per Sample is 8 (0-255)
    xFile.seek(34); unsigned int dVar = xFile.read();
    dVar = xFile.read() << 8 | dVar;
    if(dVar != 8){Serial.print("Wrong BitRate"); xFile.close(); return 0;}
    xFile.close(); return 1;
}


ISR(TIMER1_CAPT_vect){


  // The first step is to disable this interrupt before manually enabling global interrupts.
  // This allows this interrupt vector (COMPB) to continue loading data while allowing the overflow interrupt
  // to interrupt it. ( Nested Interrupts )
  // TIMSK1 &= ~_BV(ICIE1);
 //Then enable global interupts before this interrupt is finished, so the music can interrupt the buffering
  //sei();

  if(sFile.available() <= buffSize){
    buffCount = 0;
  	playing = 0;
    if(sFile){sFile.close();}
  	  *TIMSK[tt] &= ~( _BV(ICIE1) | _BV(TOIE1) );
 	  *OCRnA[tt] = 10;
      *OCRnB[tt] = resolution-10;
  }else

  for(int a=0; a<2; a++){
	  if(buffEmpty[a]){
		*TIMSK[tt] &= ~(_BV(ICIE1));
		sei();
		unsigned int tmp;
		if(volMod < 0 ){ for(int i=0; i<buffSize; i++){ tmp = (sFile.read() >> volMod*-1); buffer[a][i] = min(tmp,resolution); 	} }
		else
		for(int i=0; i<buffSize; i++){ tmp = (sFile.read() << volMod); buffer[a][i] = min(tmp,resolution); 	}
		buffEmpty[a] = 0;
	  }
  }

  if(paused){*OCRnA[tt] = 10; *OCRnB[tt] = resolution-10; *TIMSK[tt] &= ~_BV(TOIE1); } //if pausedd, disable overflow vector and leave this one enabled
  else
  if( playing ){
		  *TIMSK[tt] |= ( _BV(ICIE1) | _BV(TOIE1) );

  }

}


ISR(TIMER1_OVF_vect){

  loadCounter = !loadCounter;
  if(loadCounter == 0){ return; }

  *OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount];
  buffCount++;

  if(buffCount >= buffSize){
      buffCount = 0;
      buffEmpty[whichBuff] = true;
      whichBuff = !whichBuff;
  }

}


void TMRpcm::stopPlayback(){
  playing = 0;
  if(sFile){sFile.close();}
  *TIMSK[tt] &= ~( _BV(ICIE1) | _BV(TOIE1) );
  *OCRnA[tt] = 10;
  *OCRnB[tt] = resolution-10;

}

void TMRpcm::disable(){
  playing = 0;
  if(sFile){sFile.close();}
  *TIMSK[tt] &= ~( _BV(ICIE1) | _BV(TOIE1) );
  *TCCRnB[tt] &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12) );

}


boolean TMRpcm::isPlaying(){
	return playing;
}

