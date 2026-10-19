
#include <stdio.h>
#include "xil_printf.h"



// From Vivado's "Address Editor" tab
#define AXI_GPIO_0_BASE_ADDR 0x40000000
#define AXI_GPIO_1_BASE_ADDR 0x40010000


// Define the addresses
#define GREEN_LEDS_BASE_ADDR (AXI_GPIO_0_BASE_ADDR)
#define PUSH_BTNS_BASE_ADDR  (AXI_GPIO_0_BASE_ADDR + 8)
// ^^^ Give the Green LEDs 8 bytes (probably not 8 ints worth of space, since #define hexNum has no reason to be interpreted as a pointer) worth of space
#define RGB_LEDS_BASE_ADDR      (AXI_GPIO_1_BASE_ADDR)
#define DIP_SWITCHES_BASE_ADDR  (AXI_GPIO_1_BASE_ADDR + 8)

// Define the integers/registers
#define GREEN_LEDS_REG (unsigned *)(GREEN_LEDS_BASE_ADDR)
#define PUSH_BTNS_REG  (unsigned *)(PUSH_BTNS_BASE_ADDR)
#define RGB_LEDS_REG      (unsigned *)(RGB_LEDS_BASE_ADDR)
#define DIP_SWITCHES_REG  (unsigned *)(DIP_SWITCHES_BASE_ADDR)



void assertValidBitIdxRange(const unsigned int bitIdx){
  if( bitIdx >= sizeof(unsigned int)*8) // = numBytesPerInt*numBitsPerByte
  {xil_printf("ERROR: InvalidBitIdx: bitIdx was %d, which exceeded an unsigned int's bit width.\r\n", bitIdx);}
}
void assertValidBitTripletIdxRange(const unsigned int lowerBitIdx){
  if( lowerBitIdx >= sizeof(unsigned int)*8) // = numBytesPerInt*numBitsPerByte
  {xil_printf("ERROR:  InvalidBitTripletIdx: lowerBitIdx+2 was %d, which exceeded an unsigned int's bit width.\r\n", lowerBitIdx+2);}
}

// E.g., if bitIdx is 2, then returns either 0100 or 0000
unsigned int getBit(unsigned int *value, const unsigned int bitIdx){
  assertValidBitIdxRange(bitIdx);
  
  // (*value) below is the dereferenced pointer, i.e., the typical value rather than a memory address.
  return (*value) & (1<<bitIdx);
}






// All4LEDsGreen = 010 010 010 010 -> 0100_1001_0010 = 0x492
// All4LEDsRed = 100 100 100 100 -> 1001_0010_0100 = 0x924
//
// All4LEDsBlue = 001 001 001 001 -> 0010_0100_1001 = 0x249
// All4LEDsYellow (Yellow=Red+Green) = 110 110 110 110 -> 1101_1011_0110 = 0xDB6
// All4LEDsWhite (White=Red+Green+Blue) = 111 111 111 111 -> 0xFFF
#define FOUR_LEDs_GREEN 0x492
#define FOUR_LEDs_RED 0x924
#define FOUR_LEDs_BLUE 0x249
#define FOUR_LEDs_YELLOW 0xDB6
#define FOUR_LEDs_WHITE 0xFFF
#define FOUR_LEDs_OFF 0x0

#define FLAG_NO_ERROR 0
#define FLAG_NONEXISTENT_STATE 1
#define FLAG_INVALID_STATE__SWITCHCASE 2



typedef enum{
	INIT,
	RED,
	GREEN,
	FLASH_RED_START /*FRS. This is BEFORE the SolidRed happens.*/,
	FLASH_RED_END /*FRE. This is AFTER the SolidRed happens.*/
} TL_COLOR;//TRAFFIC_LIGHT_COLOR;

const char* getStateFromEnum(TL_COLOR color){
	switch(color){
		case INIT:	return "INIT";
		case RED:	return "RED";
		case GREEN:	return "GREEN";
		case FLASH_RED_START:	return "FLASH_RED_START";
		case FLASH_RED_END:		return "FLASH_RED_END";
		default:	return "UNKNOWN_STATE";
	}
	xil_printf("[FATAL ERROR]\r\n* getStateFromEnum():  Switch statement has somehow been bypassed.\r\n");
	xil_printf("* Did you forget to add 1) a default case, or 2) any `break;` statements if not using a `return`?\r\n");
	xil_printf("* Input state was %d\r\n\r\n", color);
	return "getStateFromEnum(TL_COLOR)::UNKNOWN_STATE";
}

const char* getErrorStringFromExitCode(const int exitCode){
	char* errAsStr = "UNKNOWN_ERROR_CODE";
	
		 if(exitCode==FLAG_NO_ERROR)
		{errAsStr  = "FLAG_NO_ERROR";}
	else if(exitCode==FLAG_INVALID_STATE__SWITCHCASE)
		{errAsStr  = "FLAG_INVALID_STATE__SWITCHCASE";}
	else if(exitCode==FLAG_NONEXISTENT_STATE)
		{errAsStr  = "FLAG_NONEXISTENT_STATE";}
	
	return errAsStr;
}

struct TrafficController{
	TL_COLOR currTrafLightColor;// = TRAFFIC_LIGHT_COLOR.INIT;	// currState
	// ^^^ This is only used for validation of state-to-state transitions. Not critical to functionality, only debugging.
	TL_COLOR nextState;//      = TRAFFIC_LIGHT_COLOR.INIT;
	unsigned int *rgbData;// = RGB_LEDS_REG;
	// ^^^ Will store the chip's location that corresponds to the RGB LEDs. Assigning to this dereferenced variable should modify the real LEDs' brightnesses/values.
} TC;	// Creates an object named `TC`



unsigned runStateTransitionAssertions(struct TrafficController* trafficController, unsigned DEBUG_LVL){
	unsigned numRulesMatched = 0;

	// Convenience variables
	struct TrafficController tc = *trafficController;	// PtrTo(tc) ==> tc
	TL_COLOR curr = tc.currTrafLightColor;
	TL_COLOR next = tc.nextState;

	// NON COMPREHENSIVE CHECKING OF ERRORS
	if(curr==GREEN && next==RED){
		if(DEBUG_LVL>0){
			xil_printf("[ X ]   runStateTransitionAssertions: State GREEN should never *directly* transition to state RED.\r\n");
			xil_printf("        There must be a \"watch out\" period between RED and GREEN.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==RED && next==GREEN){
		if(DEBUG_LVL>0){
			// Ehh this one is debatable as to its usefulness. I think RED should possibly be able to transition DIRECTLY to GREEN, a belief which disobeys this assertion.
			xil_printf("[ X ]   runStateTransitionAssertions: State RED should never *directly* transition to state GREEN.\r\n");
			xil_printf("        There must be a \"watch out\" period between RED and GREEN.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==RED && next==FLASH_RED_START){
		if(DEBUG_LVL>0){
			xil_printf("[ X ]   runStateTransitionAssertions: State RED should never transition to state FLASH_RED_START.\r\n");
			xil_printf("        FLASH_RED_START should always occur right *before* state RED, never after.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==FLASH_RED_START && next==FLASH_RED_END){
		if(DEBUG_LVL>0){
			xil_printf("[ X ]   runStateTransitionAssertions: State FLASH_RED_START should never transition *directly* to state FLASH_RED_END.\r\n");
			xil_printf("        FLASH_RED_START should always occur right *before* state RED, never after. Then, RED should always transition to FLASH_RED_END.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==FLASH_RED_END && next==FLASH_RED_START){
		if(DEBUG_LVL>0){
			xil_printf("[ X ]   runStateTransitionAssertions: State FLASH_RED_END should never transition to state FLASH_RED_START.\r\n");
			xil_printf("        FLASH_RED_START should always occur right *before* state RED, never after. Then, RED should always transition to FLASH_RED_END.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==FLASH_RED_START && next==GREEN){
		if(DEBUG_LVL>0){
			xil_printf("[ X ]   runStateTransitionAssertions: State FLASH_RED_START should never transition to state GREEN.\r\n");
			xil_printf("        FLASH_RED_START should always occur right *before* state RED, never after.\r\n");
			xil_printf("        Then, RED should always transition to FLASH_RED_END.\r\n");
			xil_printf("        Then, FLASH_RED_END should always transition to GREEN.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr!=INIT && next==INIT){
		if(DEBUG_LVL>0)
			{xil_printf("[ X ]   runStateTransitionAssertions: Non-INIT state (%s) should never transition to state INIT.\r\n", getStateFromEnum(curr));}
		numRulesMatched++;
	}
	

	// COMPREHENSIVE CHECKING OF CORRECT TRANSITIONS
	if(curr==INIT && (next==GREEN || next==FLASH_RED_START)){
		if(DEBUG_LVL>2)
			{xil_printf("[ :D ]  runStateTransitionAssertions: State INIT should always transition to either state GREEN or state FLASH_RED_START. Good.\r\n");}
		numRulesMatched++;
	}
	if(curr==RED && next==FLASH_RED_END){
		if(DEBUG_LVL>2)
			{xil_printf("[ :D ]  runStateTransitionAssertions: State RED should always transition to state FLASH_RED_END. Good.\r\n");}
		numRulesMatched++;
	}
	if(curr==FLASH_RED_START && (next==RED || next==FLASH_RED_START)){
		if(DEBUG_LVL>2)
			{xil_printf("[ :D ]  runStateTransitionAssertions: State FLASH_RED_START should always transition to either state FLASH_RED_START or state RED. Good.\r\n");}
		numRulesMatched++;
	}
	if(curr==RED && next==FLASH_RED_END){
		if(DEBUG_LVL>2)
			{xil_printf("[ :D ]  runStateTransitionAssertions: State RED should always transition to state FLASH_RED_END. Good.\r\n");}
		numRulesMatched++;
	}
	if(curr==FLASH_RED_END && next==GREEN){
		if(DEBUG_LVL>2)
		{	xil_printf("[ :D ]  runStateTransitionAssertions: State FLASH_RED_END should always transition to state GREEN. Good.\r\n");}
		numRulesMatched++;
	}

	// Struct doesn't store enough data to validate this, hence the "sometimes" disclaimer in this block.
	if(curr==GREEN && next==FLASH_RED_START){
		if(DEBUG_LVL>1){
			xil_printf("[ :) ]  runStateTransitionAssertions: State GREEN should SOMETIMES transition to state FLASH_RED_START.\r\n");
			xil_printf("        * It should happen when a button was pressed.\r\n\r\n");
		}
		numRulesMatched++;
	}
	if(curr==GREEN && next==GREEN){
		if(DEBUG_LVL>1){
			xil_printf("[ :) ]  runStateTransitionAssertions: State GREEN should SOMETIMES transition to state GREEN.\r\n");
			xil_printf("        * It should happen when a button was NOT pressed.\r\n\r\n");
		}
		numRulesMatched++;
	}

	// For copy-pasting.
	// if(curr== && next==){
	// 	xil_printf("runStateTransitionAssertions: \r\n");
	// }

	// Regardless of DEBUG_LVL's value:
	if(numRulesMatched==0){
		xil_printf("[ X ]   runStateTransitionAssertions: NO state transitions matched. Add more test cases.\r\n");
		xil_printf("        currState=%s, nextState=%s\r\n\r\n", getStateFromEnum(curr), getStateFromEnum(next));
	}
	if(numRulesMatched>1){
		xil_printf("[ X ]   runStateTransitionAssertions: MULTIPLE state transitions matched simultaneously.\r\n");
		xil_printf("        numRulesMatched=%d (at DEBUG_LEVEL %d).  currState=%s, nextState=%s\r\n\r\n",
			numRulesMatched, DEBUG_LVL, getStateFromEnum(curr), getStateFromEnum(next));
	}
	
	return numRulesMatched;
}




void delay_ms(int ms){
	for(int i=0; i < 160 * ms;  ++i)
		{asm("nop");} 
}

void test__delay_ms(int ms){
	xil_printf("test__delay_ms(int ms):\r\n* This will tell you how to tune the internal delay within function delay_ms(int ms).\r\n");
	xil_printf("* Get out a stopwatch on your phone and start it whenever you see START, and end your phone's stopwatch whenever you see END.\r\n\r\n");
	delay_ms(3000);	// Give the user some time (doesn't need to be precise) to read the above instructions
	xil_printf("START TIMER: %d ms\r\n...\r\n", ms);
	delay_ms(ms);
	xil_printf("END TIMER\r\n\r\n");
}




unsigned int atLeastOneButtonIsPushed(){
	return (*PUSH_BTNS_REG);	// MemAddressOfButtonsRegister -> ButtonData
}

unsigned int getButtonPushed(){
	// Returns the lowest-index button that is pushed.
	// If NO button is pushed, returns MAX_UNSIGNED_INT (`~0`).

	unsigned int buttonIndices = (*PUSH_BTNS_REG);

	if(buttonIndices & 0x1)
		{return 0;}
	else if(buttonIndices & 0x2)
		{return 1;}
	else if(buttonIndices & 0x4)
		{return 2;}
	else if(buttonIndices & 0x8)
		{return 3;}
	
	return ~0;
}




/* @param{numSeconds}
 * * To show the dimmed LED for as short a time as possible, provide a negative float value.
 * 
 * @param{dimmingAmount_offset}:
 * * Large positive int -> More dimming = Darker LED.
 * * Small positive int -> Less dimming = Brighter LED.
 * * Slightly negative int -> Even less dimming = Even brighter LED.
 */
void showDimmedLED(struct TrafficController *trafficController,  unsigned int colorOf4LEDs,  float numSeconds, int dimmingAmount_offset){
	// Things are too hard to configure if user gets to influence ms_LEDsSpendOn.
	//   This is why dimmingAmount_offset is *only* applied to the OFF part and not the ON part.
	const unsigned int ms_LEDsSpendOn = 3;
	int _ms_LEDsSpendOff = 10+dimmingAmount_offset;
	int ms_LEDsSpendOff;
	
	// Clip the amount that the user can modify the brightnesses. Ensure timeOff is between 0 and 25
	const int MIN_MILLIS_SPENT_OFF = 0;
	const int MAX_MILLIS_SPENT_OFF = 25;
	_ms_LEDsSpendOff  =  (_ms_LEDsSpendOff<MIN_MILLIS_SPENT_OFF)  ?  MIN_MILLIS_SPENT_OFF : _ms_LEDsSpendOff;	// max(0,timeOff)
	 ms_LEDsSpendOff  =  (_ms_LEDsSpendOff>MAX_MILLIS_SPENT_OFF)  ?  MAX_MILLIS_SPENT_OFF : _ms_LEDsSpendOff;	// min(25, timeOff)
	
	const char DEBUG = 0;
	if(  DEBUG
	  || ms_LEDsSpendOff <= MIN_MILLIS_SPENT_OFF	// The "=" is intentional.
	  || ms_LEDsSpendOff >= MAX_MILLIS_SPENT_OFF
	  || dimmingAmount_offset < -10
	  || dimmingAmount_offset > 15)
		{xil_printf("showDimmedLED:    ms_LEDsSpendOff={ (10+(offset=%d)) -> (%d) }\r\n", dimmingAmount_offset, ms_LEDsSpendOff);}
	
	// 1000ms / (3on+10off)  ==>  ~~77
	const unsigned int multiplierToGetUnitsOfSeconds = 1000/(ms_LEDsSpendOn + ms_LEDsSpendOff);
	
	// Ensure that at least one showing/lighting/displaying of LEDs occurs
	//   if user provides a negative duration for time.
	if(numSeconds < 0.0)
		{numSeconds = 1.0/multiplierToGetUnitsOfSeconds;}
	
	for(float i=0.0; i < multiplierToGetUnitsOfSeconds * numSeconds; i+=1){
		*(trafficController -> rgbData) = colorOf4LEDs;
		delay_ms(ms_LEDsSpendOn);
		
		// Dimming section:
		*(trafficController -> rgbData) = FOUR_LEDs_OFF;
		delay_ms(ms_LEDsSpendOff);
	}
}

void test__showDimmedLED(struct TrafficController* tc){
	unsigned int colorData = FOUR_LEDs_RED;
	const float numSecondsToShowTestedBrightness=1.5, numSecondsToShowReferenceBrightness=0.5;
	const int DIMMEST_OFFSET = -10;
	
	const int arrayLen = 11;
	// int *dimmingOffsetsToTest = {500, 25, 20, 15, 10, 5, 0, -5, -10 ,-15, -20};	// WILL COMPILE BUT WILL BE WRONG
	// int dimmingOffsetsToTest[arrayLen] = {500, 25, 20, 15, 10, 5, 0, -5, -10 ,-15, -20};	// WILL NOT COMPILE
	int dimmingOffsetsToTest[] = {500, 25, 20, 15, 10, 5, 0, -5, -10 ,-15, -20};
	
	
	for(int i=0; i<arrayLen; ++i){
		const int dimmingOffset = dimmingOffsetsToTest[i];
		xil_printf("test__showDimmedLED:  dimmingOffset = %d\r\n", dimmingOffset);
		showDimmedLED(tc, colorData, numSecondsToShowTestedBrightness, dimmingOffset);
		showDimmedLED(tc, colorData, numSecondsToShowReferenceBrightness, DIMMEST_OFFSET);
	}
}

void showYellowLEDs(struct TrafficController *trafficController, const float numSeconds){
	// Since green is seen more strongly by human eye, either reduce amount of green or increase amount of red
	// Since can't alter amount directly, alter the amount of time each color gets to spend being on.
	// Ideal ratio for the physical LEDs I have seems to be 3 parts red to 2 parts green
	//   (as is done below by 1red + 2red+2green).
	for(float i=0.0; i<77*numSeconds; ++i){	// 1000ms / (1+2+10)  ==>  ~~77
		*(trafficController -> rgbData) = FOUR_LEDs_RED;	// Disable green for a bit
		delay_ms(1);
		*(trafficController -> rgbData) = FOUR_LEDs_YELLOW;	// Re-enable green for a bit
		delay_ms(2);
		*(trafficController -> rgbData) = FOUR_LEDs_OFF;	// Dimming amount.
		delay_ms(10);
	}
}

void showShortLEDraveTransition(struct TrafficController *trafficController){
	// Make it clear that a new phase of blinking is starting by very quickly turning off then on then off any LEDs that are still on.
	xil_printf("\r\nTransitioning states...\r\n");
	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	delay_ms(100);
	*(trafficController -> rgbData) = FOUR_LEDs_WHITE;
	delay_ms(5);
	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	delay_ms(10);
	*(trafficController -> rgbData) = FOUR_LEDs_BLUE;
	delay_ms(10);
	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	delay_ms(10);
	*(trafficController -> rgbData) = FOUR_LEDs_YELLOW;
	delay_ms(25);
	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	xil_printf("Ended state transition.\r\n\r\n");
}






void blinkLEDsOnce(struct TrafficController *trafficController,
				unsigned colorOf4LEDs,
				const int numBlinksPerSecond,
				const int dimmingAmount){
	const char DEBUG = 0;
	if(DEBUG){xil_printf("blinkLEDsOnce\r\n");}	// DEBUG for assessing timing accuracy
	
	// while(timeElapsed < 6seconds){  Red->Off}
	// *((*trafficController).rgbData) = FOUR_LEDs_RED;		// PtrTo(tc) ==> tc ==> PtrTo(tc.rgbData) ==> tc.rgbData

	const unsigned int ms_LEDsSpendOn  = 1000/2 / numBlinksPerSecond;
	const unsigned int ms_LEDsSpendOff = 1000/2 / numBlinksPerSecond;
	if(DEBUG){xil_printf("* LEDs->ON\r\n");}
	// Non-dimmed:
	// *(trafficController -> rgbData) = colorOf4LEDs;
	// delay_ms(ms_LEDsSpendOn);
	//
	// Dimmed:
	showDimmedLED(trafficController, colorOf4LEDs, (float)(ms_LEDsSpendOn)/1000.0, dimmingAmount);
	
	if(DEBUG){xil_printf("* LEDs->OFF\r\n\r\n");}
	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	delay_ms(ms_LEDsSpendOff);
}






void uninterruptiblyBlinkLEDs(struct TrafficController *trafficController,
							unsigned colorOf4LEDs,
							const int numTotalSecondsToBlink,
							const int numBlinksPerSecond,
							const int dimmingOffset){
	// If you're wondering why the below loop isn't instead moved into/inside the blinkLEDsOnce() function, it's
	//    because of polling for whether any buttons are pressed (when in state FLASH_RED_START).
	for(int i=0; i < numTotalSecondsToBlink; ++i)
		{blinkLEDsOnce(trafficController, colorOf4LEDs, numBlinksPerSecond, dimmingOffset);}
}

// Function returns TL_COLOR nextState.
TL_COLOR blinkLEDsUntilButtonPress(struct TrafficController *trafficController,
										unsigned colorOf4LEDs,
										const int numTotalSecondsToBlink,
										const int numBlinksPerSecond,
										TL_COLOR stateToReturnUponButtonPress,
										TL_COLOR stateToReturnUponNoButtonPresses){
	const char* currState = getStateFromEnum( trafficController->currTrafLightColor );
	
	// Check every now and then for user pressing a button.
	// If user presses any button, then reset the FLASH_RED countdown (to what it originally started counting down from)
	//   by exiting this function and returning the state that should be entered for the next "tick".
	
	// ==== ATTEMPT 1 ====
	// for(int i=0; i < numSecondsToBlink * numTimesPerSecondToCheckWhetherButtonIsPressed; ++i){
	// 	if(atLeastOneButtonIsPushed()){
	// 		xil_printf("[INFO]   FSM_tick::case%s::blinkLEDsUntilButtonPress:\r\n* Button %d has been pushed.\r\n* Setting nextState = FLASH_RED_START.\r\n\r\n", currState, getButtonPushed());
	// 		return stateToReturnUponButtonPress;	// return nextState where nextState = FLASH_RED_START;
	// 	}
	// 	// ISSUE WITH BELOW LINE: Checks once per ON_THEN_OFF period, meaning that to check for a button press more frequently, you need to increase the blinking speed, possibly causing a seizure in some people.
	// 	// else{ blinkLEDsOnce(trafficController, colorOf4LEDs, numBlinksPerSecond);}
	// }
	
	// ==== ATTEMPT 2 ====
	// THE FIX: Checks any # of times per ON stage *AND* any # of times per OFF stage
	//          This moves away from numTimesToCheckButtonsPerSecond (is now unnecessary due to non-tunable parameter 134000)
	//            to numBlinksPerSecond
	// for(int blinkCnt=0; blinkCnt < numTotalSecondsToBlink; ++blinkCnt){
	// 	// CHANGE/TUNE ME! :)
	// 	// Arbitrarily-guessed time elapsed: 175 * 1000ms
	// 	// Convert from #blinks/sec  to  numTimesToCheckButtonsPer__LEDs_ON_THEN_OFF__period
	// 	int numTimesToCheckButtonsPerBlink = 110000/numBlinksPerSecond;
		
	// 	int numTimesCheckedButtons = 0;
	// 	// Single blink's LEDs_ARE_ON:
	// 	*(trafficController -> rgbData) = colorOf4LEDs;
	// 	while(numTimesCheckedButtons++ < numTimesToCheckButtonsPerBlink/2){
	// 		if(atLeastOneButtonIsPushed()){
	// 			xil_printf("[INFO]   FSM_tick::case%s::blinkLEDsUntilButtonPress::LEDs_ARE_ON:\r\n* Button %d has been pushed.\r\n* Setting nextState = FLASH_RED_START.\r\n\r\n", currState, getButtonPushed());
	// 			return stateToReturnUponButtonPress;	// return nextState where nextState = FLASH_RED_START;
	// 		}
	// 		// NO MORE CALLING OF A DELAY FUNCTION.
	// 		//   THE CHECKING (OF WHETHER ANY BUTTONS ARE BEING PUSHED) NOW SERVES AS THE DELAY.
	// 		// delay_ms(1000/2 / numBlinksPerSecond);
	// 	}
	
	// 	// Single blink's LEDs_ARE_OFF:
	// 	*(trafficController -> rgbData) = FOUR_LEDs_OFF;
	// 	while(numTimesCheckedButtons++ < numTimesToCheckButtonsPerBlink){
	// 		if(atLeastOneButtonIsPushed()){
	// 			xil_printf("[INFO]   FSM_tick::case%s::blinkLEDsUntilButtonPress::LEDs_ARE_OFF:\r\n* Button %d has been pushed.\r\n* Setting nextState = FLASH_RED_START.\r\n\r\n", currState, getButtonPushed());
	// 			return stateToReturnUponButtonPress;	// return nextState where nextState = FLASH_RED_START;
	// 		}
	// 	}
	// }
	
	// ==== ATTEMPT 3 ====
	// I now want the LEDs to be dimmable
	for(int blinkCnt=0; blinkCnt < numTotalSecondsToBlink; ++blinkCnt){
		// CHANGE/TUNE ME! :)
		// Arbitrarily-guessed time elapsed: 175 * 1000ms
		// Convert from #blinks/sec  to  numTimesToCheckButtonsPer__LEDs_ON_THEN_OFF__period
		int numTimesToCheckButtonsPerBlink = 50000/numBlinksPerSecond;
		
		int numTimesCheckedButtons = 0;
		// Single blink's LEDs_ARE_ON:
		while(numTimesCheckedButtons++ < numTimesToCheckButtonsPerBlink/2){
			*(trafficController -> rgbData) = colorOf4LEDs;
			*(trafficController -> rgbData) = FOUR_LEDs_OFF;	// Dim to 50% power
			*(trafficController -> rgbData) = FOUR_LEDs_OFF;	// Dim to 33% power (when including the above line)
			*(trafficController -> rgbData) = FOUR_LEDs_OFF;	// Dim to 25% power (when including the above 2 lines)
			if(atLeastOneButtonIsPushed()){
				xil_printf("[INFO]   FSM_tick::case%s::blinkLEDsUntilButtonPress::LEDs_ARE_ON:\r\n* Button %d has been pushed.\r\n* Setting nextState = FLASH_RED_START.\r\n\r\n", currState, getButtonPushed());
				return stateToReturnUponButtonPress;	// return nextState where nextState = FLASH_RED_START;
			}
		}
	
		// Single blink's LEDs_ARE_OFF:
		// *(trafficController -> rgbData) = FOUR_LEDs_OFF;	// Unnecessary due to the single blink's "on" phase ending in a "off" configuration
		while(numTimesCheckedButtons++ < numTimesToCheckButtonsPerBlink){
			if(atLeastOneButtonIsPushed()){
				xil_printf("[INFO]   FSM_tick::case%s::blinkLEDsUntilButtonPress::LEDs_ARE_OFF:\r\n* Button %d has been pushed.\r\n* Setting nextState = FLASH_RED_START.\r\n\r\n", currState, getButtonPushed());
				return stateToReturnUponButtonPress;	// return nextState where nextState = FLASH_RED_START;
			}
		}
	}

	
	// Turn the LEDs back on before exiting this function so that the TrafficLight doesn't look
	//   buggy/untrustworthy by having its lights completely off.
	*(trafficController -> rgbData) = colorOf4LEDs;

	
	return stateToReturnUponNoButtonPresses;	// Only happens if all buttons were never pushed during the specified window of time (i.e., during the for() loop).
	// ^^^ nextState = RED;
}










int FSM_tick(
	struct TrafficController* trafficController,
	const int numTotalSecondsToBlink,
	const int num_onANDoffTogether_BlinksPerSecond,
	const int dimmingOffset,
	const char SHOW_RAVE_TRANSITIONS,
	const char DEBUG_FSM,
	const unsigned DEBUG_LEVEL_TRANSITION_ASSERTIONS
){
	// xil_printf("FSM_tick() has been called.\r\n");	// DEBUG
	
	
	
	TL_COLOR nextState = INIT;	// Local variable. Modifying this *alone*/*by itself* WILL NOT affect the object's (nor the LEDs') ACTUAL value.

	unsigned errSwitchCase = 0;
	volatile TL_COLOR stateToBecome_thisIteration = trafficController->nextState;
	// Each switch case below (except for case INIT) should only modify the object's rgbData field *directly* (rather than indirectly via local variable).
	//   This is due to the blinking action requiring colors to temporarily be set to allBitValues0.
	//  The object's currTrafLightColor (i.e., currState) should not be modified directly.
	switch(stateToBecome_thisIteration){
		case INIT:	// On PowerSystemUp
			// (Immediately) Directly modify the currState. (rather than waiting until the switch completes/exits)
			trafficController->currTrafLightColor = GREEN;
			trafficController->nextState = GREEN;	// Also *directly* modify nextState (only for case INIT though)

			// (Lazily) Configure next state
			nextState = GREEN;
			
			// (Immediately) Make LEDs shine SolidGreen
			// 
			trafficController->rgbData = RGB_LEDS_REG;	// Assign the POINTER (which is on the left side of the "=") to have the chip's memory address that corresponds to the 0th RGB_LED.
			// ^^^ *Should* only ever need to happen when in INIT state.
			*(trafficController->rgbData) = FOUR_LEDs_GREEN;	// Now/after that the pointer is pointing to the right place, configure the data that exists *at* that pointer.
			// ^^^ I.e., set the LED brightness data.
			
			break;
		case GREEN:	// On SafeForDriversButNotBikers
			if(atLeastOneButtonIsPushed()){
				if(DEBUG_FSM){
					xil_printf("FSM_tick::caseGREEN:\r\n* Button #%d has been pushed. (other buttons may have *also* been pushed).\r\n",  getButtonPushed());
					xil_printf("* Transitioning from state GREEN to state FLASH_RED_START.\r\n\r\n");
				}
				nextState = FLASH_RED_START;
			}
			else{
				nextState = GREEN;
				// There's no condition based on time since **this is the natural resting/default state** the TrafficLight should be in.
				// Un-dimmed:
				// *(trafficController->rgbData) = FOUR_LEDs_GREEN;
				// Dimmed:
				// showDimmedLED(trafficController, FOUR_LEDs_GREEN, 0.05, dimmingOffset);	// 0.05sec == 1/20th of a second.
				showDimmedLED(trafficController, FOUR_LEDs_GREEN, -1.0, dimmingOffset+10);
				// ^^^ To allow buttons to be checked very frequently (high/good user responsiveness),
				//     you should set this value to be pretty small since checks can only occur *after* showing the LED.
			}
			
			break;
		case FLASH_RED_START:	// On begin to flash, specifically red. DriversAboutToLoseRightOfWay
			if(SHOW_RAVE_TRANSITIONS)
				{showShortLEDraveTransition(trafficController);}
			nextState = blinkLEDsUntilButtonPress(
							trafficController,
							FOUR_LEDs_RED,
							numTotalSecondsToBlink,  num_onANDoffTogether_BlinksPerSecond,
							FLASH_RED_START,// stateToReturnUponButtonPress
							RED);			// stateToReturnUponNoButtonPresses
			
			break;
		case RED:	// On SafeForBikersButNotDrivers.
			if(SHOW_RAVE_TRANSITIONS)
				{showShortLEDraveTransition(trafficController);}
			if(DEBUG_FSM)
				{xil_printf("[INFO]  FSM_tick::caseRED:   Button presses are ignored here.\r\n");}
			
			// Stay RED for 4 seconds before switching state to FLASH_RED_END
			//
			// Non-dimmed:
			// *(trafficController->rgbData) = FOUR_LEDs_RED;
			// delay_ms(4000);
			//
			// Dimmed:
			showDimmedLED(trafficController, FOUR_LEDs_RED, 4.0, dimmingOffset);
			
			nextState = FLASH_RED_END;
			
			break;
		case FLASH_RED_END:	// On BikersAboutToLoseRightOfWay
			if(SHOW_RAVE_TRANSITIONS)
				{showShortLEDraveTransition(trafficController);}
			if(DEBUG_FSM)
				{xil_printf("[INFO]  FSM_tick::caseFLASH_RED_END:   Button presses are ignored here.\r\n");}
				
			uninterruptiblyBlinkLEDs(trafficController,
				FOUR_LEDs_RED,
				numTotalSecondsToBlink,
				num_onANDoffTogether_BlinksPerSecond,
				dimmingOffset);
			nextState = GREEN;
			break;
		
		// case default:	// WRONG SYNTAX. DO NOT INCLUDE THE WORD `case` FOR THE DEFAULT/CATCH-ALL CASE
		default:
			xil_printf("[LIKELY ERROR]\r\n* This default switch case should have NEVER been reached (unless during startup).\r\n");
			xil_printf("* stateToBecome_thisIteration=%d\r\n* Setting all 4 variables (2 object vars, 2 local vars) to state INIT.\r\n\r\n", stateToBecome_thisIteration);
			errSwitchCase = 1;
			// Default case: Set both currState and nextState to INIT, directly modifying the object AND setting local variables.
			//   Setting all 4 variables is the safety-oriented catch-all.
			trafficController->currTrafLightColor = INIT;
			trafficController->nextState = INIT;
			stateToBecome_thisIteration = INIT;
			nextState = INIT;
			
			// break;	// Unnecessary for switch's default case.
	}
	
	// xil_printf("[INFO]  Preparing currState and nextState for next iteration of FSM_tick()...\r\n");	// Commented out due to lag
	//
	unsigned objCurr, objNext, locCurr, locNext;
	if(DEBUG_FSM){
		objCurr = trafficController->currTrafLightColor;
		objNext = trafficController->nextState;
		locCurr = stateToBecome_thisIteration;
		locNext = nextState;
		xil_printf("\r\nBEFORE updating variables for next iteration:\r\n* objCurr, objNext, locCurr, locNext:   %s, %s, %s, %s\r\n",
			getStateFromEnum(objCurr), getStateFromEnum(objNext),
			getStateFromEnum(locCurr), getStateFromEnum(locNext));
	}
	//
	// ATTEMPT 1
	// // tl;dr of the below 4 lines:  obj.currState, obj.nextState   =   obj.nextState, localVariable.nextState
	// // Set currState to be the state that's about to be overwritten.
	// trafficController->currTrafLightColor = trafficController->nextState;
	// // Update the TrafficController object using the local variable `nextState`.
	// trafficController->nextState = nextState;
	//
	// ATTEMPT 2
	trafficController->currTrafLightColor = stateToBecome_thisIteration;
	trafficController->nextState = nextState;
	// I changed the beginning of FSM_tick() to use tc.nextState (for stateToBecome_thisIteration) rather than using tc.currTrafLightColor
	//
	if(DEBUG_FSM){
		xil_printf("AFTER updating variables for next iteration:\r\n* objCurr, objNext, locCurr, locNext:   %s, %s, %s, %s\r\n\r\n",
			getStateFromEnum(trafficController->currTrafLightColor), getStateFromEnum(trafficController->nextState),
			getStateFromEnum(stateToBecome_thisIteration), getStateFromEnum(nextState));
	}


	if(DEBUG_FSM)
		{xil_printf("[INFO]  Checking validity of state transitions...\r\n");}
	// Will print out whether a transition is valid or invalid.
	unsigned numRulesMatched = runStateTransitionAssertions(trafficController, DEBUG_LEVEL_TRANSITION_ASSERTIONS);

	//======== Handle errors ========
	if(numRulesMatched>1){
		xil_printf("[WARN]\r\n* Multiple (%d) transition rules were matched during state transition assertions. Expected exactly 1 match.\r\n\r\n", numRulesMatched);
	}
	// Return error code from FSM_tick(), but not before iteration update nor before state transition assertions.
	if(errSwitchCase){return FLAG_INVALID_STATE__SWITCHCASE;}

	return FLAG_NO_ERROR;
}







int main(void)
{
	// By using pointers rather than actual values, we don't need to worry about having to use `volatile`, at least in the cases done so far.
	// Data and tristate register *ptrs* for the 4 green LEDs on the Arty board
	unsigned *greenLEDsData = GREEN_LEDS_REG;
	unsigned *greenLEDsTri  = GREEN_LEDS_REG + 1;

	// Data and tristate register *ptrs* for the 4 push buttons on the Arty board
	unsigned *buttonsData = PUSH_BTNS_REG;
	unsigned *buttonsTri  = PUSH_BTNS_REG + 1;

	// Data and tristate register *ptrs* for the 4 RGB LEDs on the Arty board
	unsigned *rgbLEDsData = RGB_LEDS_REG;
	unsigned *rgbLEDsTri  = RGB_LEDS_REG + 1;

	// Data and tristate register *ptrs* for the 4 dip switches on the Arty board
	unsigned *dipSwitchesData = DIP_SWITCHES_REG;
	unsigned *dipSwitchesTri  = DIP_SWITCHES_REG + 1;

	// Set the tri-state direction of the AXI_GPIO_0 pins and AXI_GPIO_1 pins.
	// Buttons = inputs, LEDs = outputs
	// (input = high = 1,   output = low = 0)
	*greenLEDsTri = 0x0;
	*rgbLEDsTri   = 0x0;
	*buttonsTri     = 0xF;  // (hex)F = 0b1111
	*dipSwitchesTri = 0xF;

	// Used to determine if the program should print to the console
	unsigned oldButtonsData = 0;
	unsigned oldDipSwitchesData = 0;
	
	struct TrafficController trafficController;	// Create new TC object from the pre-defined class/struct.

	// ==== Just for testing/fun. ====
	// showYellowLEDs(&trafficController, 7.0);	//numSeconds
	// test__delay_ms(5000);
	// showDimmedLED(&trafficController, FOUR_LEDs_RED, 7.0, 500);


	// CHANGE ME! :)
	const int numTotalSecondsToBlink = 6;	// Total time summed across all cumulative blinks.
	const int num_onANDoffTogether_BlinksPerSecond = 1;
	// const int numTimesPerSecondToCheckWhetherButtonIsPressed = 4;	// No longer necessary :D
	const int dimmingOffset = 0;	// 0 == Default dimming amount (i.e., some dimming is applied)
	
	/*  / \                    / \
	 * / ! \ EPILEPSY WARNING / ! \ 
	 * 
	 * USING SHOW_RAVE_TRANSITIONS>0 WILL CAUSE SHORT-LIVED LED STROBE FLASHING DUE TO IT INTENTIONALLY BEING NOTICEABLE.
	 * USING DEBUG_FSM>0 WILL CAUSE LED STROBE FLASHING DUE TO EXTREME COMPUTATION LAG.
	 * USING DEBUG_LEVEL_TRANSITION_ASSERTIONS>1 WILL CAUSE LED STROBE FLASHING DUE TO COMPUTATION LAG.
	 */
	// CHANGE ME! :)
	const char SHOW_RAVE_TRANSITIONS = 1;
	const char DEBUG_FSM = 0;
	const unsigned DEBUG_LEVEL_TRANSITION_ASSERTIONS = 1;
	// ^^^ 0 == noDebugAtAll, 1 == PrintDefiniteErrors, 2 == PrintDefiniteErrorsAndPossibleErrors,
	//     3+ == maxDebug(PrintDefiniteErrorsAndPossibleErrorsAndNonErrors)
	
	while(1)
	{
		// ==== Show which buttons the user is pressing ====
		// Set the LED registers to be the same as the buttons'/dipswitches' current reading
		*greenLEDsData = *buttonsData;

		// To avoid spamming the COM terminal with each loop, let's only print if the
		// status of the buttons/dipswitches register has changed:
		if(*buttonsData != oldButtonsData)
		{
			oldButtonsData = *buttonsData;
			xil_printf("buttonsData register: 0x%08x\r\n", *buttonsData);
		}
		if(*dipSwitchesData != oldDipSwitchesData)
		{
			oldDipSwitchesData = *dipSwitchesData;
			xil_printf("dipSwitchesData register: 0x%08x\r\n", *dipSwitchesData);
		}

		// ==== Just for testing/fun. ====
		// showYellowLEDs(&trafficController, 0.25);	// numSeconds
		// showYellowLEDs(&trafficController, 25);		// numSeconds
		// test__showDimmedLED(&trafficController);
		
		// Cycle through traffic light pattern
		const unsigned exitCode = FSM_tick(
			&trafficController,	// Pass the pointer to the struct rather than passing the entire object directly.
			numTotalSecondsToBlink,
			num_onANDoffTogether_BlinksPerSecond,
			dimmingOffset,
			SHOW_RAVE_TRANSITIONS,
			DEBUG_FSM,
			DEBUG_LEVEL_TRANSITION_ASSERTIONS
		);
		
		if(exitCode){	// I.e., if(exitCode != 0)
			xil_printf("[ERROR]\r\n* Received error code %d (%s) from FSM_tick().\r\n", exitCode, getErrorStringFromExitCode(exitCode));
			// Crash the program if error occurs AND both states aren't INIT
			// * BOTH states being INIT is okay, so don't crash the program in that case.
			if(exitCode==FLAG_INVALID_STATE__SWITCHCASE
				&& !(trafficController.currTrafLightColor == INIT
				     && trafficController.nextState == INIT)
			){
				xil_printf("[ERROR WAS FATAL]  Exiting program.\r\n");
				return exitCode;
			}
		}
	}
	return 0;
}
