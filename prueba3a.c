/******************************************************************************/
/*                                                                            */
/*  Description:                                                              */
/*                                                                            */
/*  Author:                                                                   */
/*                                                                            */
/******************************************************************************/

#include <p30f4011.h>
#include <time.h>
#include <stdlib.h>
#include "can.h"

/******************************************************************************/
/* Configuration words                                                        */
/******************************************************************************/
_FOSC(CSW_FSCM_OFF & EC_PLL16);
_FWDT(WDT_OFF);
_FBORPOR(MCLR_EN & PBOR_OFF & PWRT_OFF);
_FGS(CODE_PROT_OFF);

/******************************************************************************/
/* Hardware                                                                   */
/******************************************************************************/

/******************************************************************************/
/* Constants				                                                  */
/******************************************************************************/
#define	WIDTH		80
#define	LENGTH		24

#define	BALL_L		1
#define	PADDLE_L	5
#define PADDLE_W	2

#define	PAD1_X		2
#define	PAD2_X		76

#define SERV_NO		0
#define SERV_YES	1

#define	M_BALL		00
#define	M_BOUNCE	02
#define	M_POINT		04
#define	S1_PADDLE	10
#define	S1_SERVICE	11
#define	S2_PADDLE	20
#define	S2_SERVICE	21

/******************************************************************************/
/* Global Variable declaration                                                */
/******************************************************************************/
// Ball coordenates (Range: 0-WIDTH, 0-LENGTH)
unsigned int bx, by;
// Paddle 1 and 2 top left coordinates (Range: PAD1_X, 0-(LENGTH-PADDLE_L), PAD2_X, 0-(LENGTH-PADDLE_L))
volatile unsigned int p1x, p1y, p2x, p2y;
// Ball movement vector (Values: -1.1, -1.1)
volatile int vector_x, vector_y;
// Ball speed (Range: 0-4)
volatile unsigned int speed;
// Service no/yes (Values: 0.1)
volatile unsigned char service;
// Possession service (Values: 1.2)
unsigned char pos_service;

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/
void _ISR _ADCInterrupt(void) {
	int ADCValue = ADCBUF0;		// get ADC value
	speed = ((float)ADCValue/1023.0)*4;
	IFS0bits.ADIF = 0;			// restore ADIF
}

void _ISR _C1Interrupt() {
	if (C1INTFbits.RX0IF == 1) {
		unsigned int id = C1RX0SIDbits.SID;
		switch (id) {
			case S1_PADDLE:
				p1y = C1RX0B1;
				break;
			case S1_SERVICE:
				if (pos_service == 1) {
					service = SERV_NO;
					vector_x = 1;
					vector_y = ((rand() % 2) == 0) ? -1 : 1;
				}
				break;
			case S2_PADDLE:
				p2y = C1RX0B1;
				break;
			case S2_SERVICE:
				if (pos_service == 2) {
					service = SERV_NO;
					vector_x = -1;
					vector_y = ((rand() % 2) == 0) ? -1 : 1;
				}
				break;
		}

		C1RX0CONbits.RXFUL = 0; 	// Clear reception full status flag
		C1INTFbits.RX0IF = 0;
	}
	IFS1bits.C1IF = 0;
}

/******************************************************************************/
/* Procedures                                                                 */
/******************************************************************************/
void CAN_config();
void ADC_config();
void master_init();
unsigned char check_paddle_hit();
unsigned char check_wall_hit();
int check_point_made();

int main(void)
{
	CAN_config();
	ADC_config();
	
	master_init();
	
	// mode: 0->nothing, 1->bounce, 2->point
	int mode, winner, i;
	unsigned int ball_coordinates[2];
	while (1) {
		mode = 0;
		winner = 0;
		
		// Update ball coordinates
		if (service) {
			if (pos_service == 1) {
				bx = p1x + (PADDLE_W) + 1;
				by = p1y + (PADDLE_L/2);
			} else {
				bx = p2x - 2;
				by = p2y + (PADDLE_L/2);
			}
		} else {
			bx += vector_x;
			by += vector_y;
		}
		
		// Check bounces
		unsigned char paddle_bounce = check_paddle_hit();
		if (paddle_bounce) {
			mode = 1;
			vector_x = (vector_x == 1) ? -1 : 1;
			if (bx < (WIDTH/2)) {	// If it bounced with paddle 1
				if (vector_y == 1 && by < (p1y + (PADDLE_L/2))) vector_y = -1;
				else if (vector_y == -1 && by > (p1y + (PADDLE_L/2))) vector_y = 1;
			} else {				// If it bounced with paddle 2
				if (vector_y == 1 && by < (p2y + (PADDLE_L/2))) vector_y = -1;
				else if (vector_y == -1 && by > (p2y + (PADDLE_L/2))) vector_y = 1;
			}
		}
		unsigned char wall_bounce = check_wall_hit();
		if (wall_bounce) {
			mode = 1;
			vector_y = (vector_y == -1) ? 1 : -1;
		}
		
		// Check if someone has scored
		winner = check_point_made();
		if (winner) {
			mode = 2;
			if (winner == 1) {
				bx = p2x - 1;
				by = p2y + (PADDLE_L/2) - BALL_L;
				pos_service = 2;
			} else {
				bx = p1x + (PADDLE_W) + 1;
				by = p1y + (PADDLE_L/2) - BALL_L;
				pos_service = 1;
			}
			service = SERV_YES;
		}
		
		// Send messages
		if (mode == 1) CANSendMsg(M_BOUNCE, 0, NULL);
		else if (mode == 2) CANSendMsg(M_POINT, 1, &winner);
		ball_coordinates[0] = bx;
		ball_coordinates[1] = by;
		CANSendMsg(M_BALL, 2, ball_coordinates);
		
		// Wait until next update
		for (i = 0; i < 100-20*speed; i++) Delay5ms();
	}
	
    return 0;
}

void CAN_config() {
	/* Initialize CAN */
	C1CTRLbits.REQOP = 0b100;          	// Set configuration mode
	while(C1CTRLbits.OPMODE != 0b100); 	// Wait until configuration mode

	C1CTRLbits.CANCKS = 1; 				// FCAN = FCY

	/* Baud rate */

	// BTR config 1
	C1CFG1bits.BRP = 0;					// Prescaler to 2/Fcan
	C1CFG1bits.SJW = 0;					// 1 TQ

	// BTR config 2
	C1CFG2bits.PRSEG  = 0b000; 			// 1 TQs
	C1CFG2bits.SEG1PH = 0b011; 			// 4 TQs
	C1CFG2bits.SEG2PH = 0b001; 			// 2 TQs

	/* Interrupts */

	// General CAN interrupt
	IEC1bits.C1IE = 1; 			// Enable general CAN interrupt
	IFS1bits.C1IF = 0; 			// Clear general CAN interrupt flag

	// Local CAN interrupts
	C1INTEbits.RX0IE = 1; 		// Enable CAN interrupt associated to rx buffer 0
	C1INTFbits.RX0IF = 0; 		// Clear CAN interrupt flag associated to rx buffer 0
	
	/* Tx buffer 0 */

	// General transmission configuration
	C1TX0CONbits.TXREQ = 0; 	// Clear transmission request flag

	/* Rx buffer 0 */
	
	// General reception configuration
	C1RX0CONbits.RXFUL = 0; 	// Clear reception full status flag
	C1RX0CONbits.DBEN = 0; 		// Disable double buffer

	// Configure acceptance mask
	C1RXM0SIDbits.SID = 0; 		// No bits to compare
	C1RXM0SIDbits.MIDE = 1; 	// Identifier mode as determined by EXIDE
	C1RX0CONbits.FILHIT0 = 0; 	// Link to acceptance filter 0

	// Configure acceptance filters
	C1RXF0SIDbits.EXIDE = 0; 	// Enable filter for standard identifier
	C1RXF0SIDbits.SID = 0; 		// Doesn't matter the value as mask is '0'

	C1CTRLbits.REQOP = 0b000;			// Set normal mode
	while(C1CTRLbits.OPMODE != 0b000);	// Wait until normal mode
}

void ADC_config() {
	TRISBbits.TRISB7 = 1;	// Declare AN7 as input
	ADPCFG = 0xFF7F;		// all PORTB = Digital; RB7 = analog
	ADCON1 = 0x00E4;		// SAMP bit is auto-set
							// sampling begins immediately after last conversion
	ADCHS = 0x0007;			// Connect RB7/AN7 as CH0 input ..
	ADCSSL = 0;
	ADCON3 = 0x0000;
	
	// Set ADC clock 32 times slower than System clock
	ADCON3bits.ADCS = 31;
	
	// Set time bewteen conversions to 31 ADC clock cycles
	ADCON3bits.SAMC = 31;
	ADCON2 = 0;
	
	// Set at 16 the number of conversions before throwing an interruption
	ADCON2bits.SMPI = 0b1111;
	ADCON1bits.ADON = 1; 	// turn ADC ON
	ADCON1bits.SAMP = 1;	// start sampling
	
	//Configure interrupts
	IEC0bits.ADIE = 1;		// enable ADC interrupt requests
	IFS0bits.ADIF = 0;		// clear ADIF bit
}

void master_init() {
	srand(time(NULL));
	// Initial service
	service = SERV_YES;
	pos_service = 1;
	
	// Initial paddle coordinates
	p1x = PAD1_X;
	p1y = (LENGTH/2) - (PADDLE_L/2);
	p2x = PAD2_X;
	p2y = (LENGTH/2) - (PADDLE_L/2);
	
	// Initial ball coordinates
	bx = p1x + (PADDLE_W) + 1;
	by = p1y + (PADDLE_L/2);
	
	// Initial ball speed
	speed = 0;
	
	// Initial ball movement vector
	vector_x = 1;
	vector_y = ((rand() % 2) == 0) ? -1 : 1;
}

/* Checks if the ball hit a paddle
 * return: 0, if it didn't hit
 * 		   1, otherwise
 */
unsigned char check_paddle_hit() {
	unsigned char hit = 0;
	
	if (bx <= p1x+PADDLE_W && by >= p1y && by < p1y+PADDLE_L)
		hit = 1;
	if (bx >= p2x && by >= p2y && by < p2y+PADDLE_L)
		hit = 1;
	
	return hit;
}

/* Checks if the ball hit a wall
 * return: 0, if it didn't hit
 * 		   1, otherwise
 */
unsigned char check_wall_hit() {
	unsigned char hit = 0;
	
	if (by <= 0 || by >= LENGTH)
		hit = 1;
	
	return hit;
}

/* Checks if a point was made
 * return: 0, if no point made
 * 		   1-2, the winner
 */
int check_point_made() {
	unsigned char point = 0;
	
	if (bx <= 0) point = 2;
	else if (bx >= WIDTH) point = 1;
	
	return point;
}
