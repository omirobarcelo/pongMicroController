/******************************************************************************/
/*                                                                            */
/*  Description:                                                              */
/*                                                                            */
/*  Author:                                                                   */
/*                                                                            */
/******************************************************************************/

#include <p30f4011.h>
#include <uart.h>

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

#define FXT       7372800         // CPU clock
#define PLL       16              // PLL configuration
#define FCY       (FXT * PLL) / 4 // Clock that feeds the UART

#define BAUD_RATE 115200
#define BRG       (FCY / (16L * BAUD_RATE)) - 1L

/******************************************************************************/
/* Constants				                                                  */
/******************************************************************************/
#define	WIDTH		80 //40
#define	LENGTH		24

#define	BALL_L		1
#define	PADDLE_L	5
#define PADDLE_W	2

#define	PAD1_X		2
#define	PAD2_X		76 //36

#define SCORE1_X	31 //11
#define SCORE2_X	44 //24
#define SCORE_Y		1

#define UP			'i'
#define DOWN		'k'
#define BUZZ		'j'

#define FILL		"#"

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
unsigned int p1x, p1y, p2x, p2y;
// Scoreboard
unsigned int score[2];
// Previous versions of ball and paddle variables
unsigned int pre_bx, pre_by, pre_p1y, pre_p2y;
// Current screen cursor position (Range: 0-WIDTH, 0-LENGTH)
unsigned int cx, cy;

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/
void _ISR _U1RXInterrupt() {
	unsigned char c = ReadUART1();
	//unsigned char c = U1RXREG & 0xFF;
	//if (U1MODEbits.PDSEL == 3) c = U1RXREG;
	//else c = U1RXREG & 0xFF;
	//unsigned char str[5];
	//sprintf(str, "%u", c);
	//WriteUART1(c);
	//while (BusyUART1());
	
	if (c == UP) if (p1y > 0) {
		pre_p1y = p1y; p1y -= 1; 
		//WriteUART1(105);
		//while (BusyUART1());
		CANSendMsg(S1_PADDLE, 1, &p1y);
		//WriteUART1(105);
		//while (BusyUART1());
	}
	if (c == DOWN) if (p1y < LENGTH-PADDLE_L) {
		pre_p1y = p1y; p1y += 1;
		//WriteUART1(105);
		//while (BusyUART1());
		CANSendMsg(S1_PADDLE, 1, &p1y);
		//WriteUART1(105);
		//while (BusyUART1());
	}
	if (c == BUZZ) {
		WriteUART1(7);			// Send the buzzer character back to the UART
		while (BusyUART1());	// Wait until the character is transmitted
	}
	
	IFS0bits.U1RXIF = 0;
}

void _ISR _C1Interrupt() {
	if (C1INTFbits.RX0IF == 1) {
		WriteUART1(105);
		while (BusyUART1());
		unsigned int id = C1RX0SIDbits.SID;
		switch (id) {
			case S2_PADDLE:
				pre_p2y = p2y;
				p2y = C1RX0B1;
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
void UARTConfig();
void CAN_config();
void slave1_init();
void clear_screen();
void draw_screen();

int main(void){
	UARTConfig();
	CAN_config();
	
	slave1_init();
	
	int j;
	for (j = 0; j < 1600; j++) Delay5ms();
	clear_screen();
	draw_screen();
	while (1) {
		if (pre_bx != bx || pre_by != by || pre_p1y != p1y || pre_p2y != p2y)
			draw_screen();
	}
	
	return 0;
}

void UARTConfig(){
	U1MODE = 0;                     // Clear UART config - to avoid problems with bootloader

	// Config UART
	OpenUART1(UART_EN &             // Enable UART
			  UART_DIS_LOOPBACK &   // Disable loopback mode
			  UART_NO_PAR_8BIT &	// 8bits / No parity
			  UART_1STOPBIT,		// 1 Stop bit

			  UART_TX_PIN_NORMAL &  // Tx break bit normal
			  UART_TX_ENABLE,       // Enable Transmition

			  BRG);                 // Baudrate
	U1STAbits.URXISEL = 0;
			  
	// Enable UART Rx Interrupts
	IEC0bits.U1RXIE = 1;
	IFS0bits.U1RXIF = 0;
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
	C1RXM0SIDbits.SID = 0;	// Mask to check if sid is odd or even
	C1RXM0SIDbits.MIDE = 1; 			// Identifier mode as determined by EXIDE
	C1RX0CONbits.FILHIT0 = 0; 			// Link to acceptance filter 0

	// Configure acceptance filters
	C1RXF0SIDbits.EXIDE = 0; 			// Enable filter for standard identifier
	C1RXF0SIDbits.SID = 0; 	// Accept messages with even identifier

	C1CTRLbits.REQOP = 0b000;			// Set normal mode
	while(C1CTRLbits.OPMODE != 0b000);	// Wait until normal mode
}

void slave1_init() {
	// Initial paddle coordinates
	p1x = PAD1_X;
	p1y = (LENGTH/2) - (PADDLE_L/2);
	p2x = PAD2_X;
	p2y = (LENGTH/2) - (PADDLE_L/2);
	
	// Initial ball coordinates
	bx = p1x + (PADDLE_W) + 1;
	by = p1y + (PADDLE_L/2);// + BALL_L;
	
	// Initial scores
	score[0] = 0;
	score[1] = 0;
	
	// Initial previous values at same value
	pre_bx = bx;
	pre_by = by;
	pre_p1y = p1y;
	pre_p2y = p2y;
	
	draw_screen();
}

void clear_screen() {
	WriteUART1(12);
	while (BusyUART1()); 		// Wait until the character is transmitted
	cx = 0; cy = 0;
}

void move_up() {
	//if (cy > 0) {
		WriteUART1(27);
		while (BusyUART1());
		WriteUART1(91);
		while (BusyUART1());
		WriteUART1(65);
		while (BusyUART1());
		cy--;
	//}
}

void move_down() {
	//if (cy < WIDTH) {
		WriteUART1(27);
		while (BusyUART1());
		WriteUART1(91);
		while (BusyUART1());
		WriteUART1(66);
		while (BusyUART1());
		cy++;
	//}
}

void move_left() {
	//if (cx > 0) {
		WriteUART1(27);
		while (BusyUART1());
		WriteUART1(91);
		while (BusyUART1());
		WriteUART1(68);
		while (BusyUART1());
		cx--;
	//}
}

void move_right() {
	//if (cx < LENGTH) {
		WriteUART1(27);
		while (BusyUART1());
		WriteUART1(91);
		while (BusyUART1());
		WriteUART1(67);
		while (BusyUART1());
		cx++;
	//}
}

void draw_fill() {
	//if (cx < LENGTH) {
		putsUART1(FILL);
		while (BusyUART1());
		cx++;
	//}
}

void reset_cursor() {
	while (cx > 0) {
		move_left();
	}
	while (cy > 0) {
		move_up();
	}
}

void draw_number(unsigned int number);

void draw_screen() {
	int i, j;
	// Clear screen and reset cursor
	clear_screen();
	
	// Draw player 1 paddle
	while (cx < p1x) {
		move_right();
	}
	while (cy < p1y)  {
		move_down();
	}
	for (i = 0; i < PADDLE_L; i++) {
		for (j = 0; j < PADDLE_W; j++) {
			draw_fill();
		}
		for (j = 0; j < PADDLE_W; j++) {
			move_left();
		}
		move_down();
	}
	reset_cursor();
	
	// Draw player 2 paddle
	while (cx < p2x) {
		move_right();
	}
	while (cy < p2y)  {
		move_down();
	}
	for (i = 0; i < PADDLE_L; i++) {
		for (j = 0; j < PADDLE_W; j++) {
			draw_fill();
		}
		for (j = 0; j < PADDLE_W; j++) {
			move_left();
		}
		move_down();
	}
	reset_cursor();
	
	// Draw ball
	while (cx < bx) {
		move_right();
	}
	while (cy < by)  {
		move_down();
	}
	draw_fill();
	reset_cursor();
	
	// Draw player 1 scoreboard
	while (cx < SCORE1_X) {
		move_right();
	}
	while (cy < SCORE_Y)  {
		move_down();
	}
	draw_number(score[0]);
	reset_cursor();
	
	// Draw player 2 scoreboard
	while (cx < SCORE2_X) {
		move_right();
	}
	while (cy < SCORE_Y)  {
		move_down();
	}
	draw_number(score[1]);
	reset_cursor();
	
	//Restore preview values
	pre_bx = bx;
	pre_by = by;
	pre_p1y = p1y;
	pre_p2y = p2y;
}

void draw_number(unsigned int number) {
	switch (number) {
		// ####
		// #  #
		// #  #
		// #  #
		// ####
		case 0:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		//  ##
		//  ##
		//  ##
		//  ##
		//  ##
		case 1:
				move_right();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			break;
		// ####
		//    #
		// ####
		// #
		// ####
		case 2:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		// ####
		//    #
		// ####
		//    #
		// ####
		case 3:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		// #  #
		// #  #
		// ####
		//    #
		//    #
		case 4:
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
			break;
		// ####
		// #
		// ####
		//    #
		// ####
		case 5:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		// #
		// #
		// ####
		// #  #
		// ####
		case 6:
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		// ####
		//    #
		//    #
		//    #
		//    #
		case 7:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
			
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
			
			draw_fill();
			break;
		// ####
		// #  #
		// ####
		// #  #
		// ####
		case 8:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
			break;
		// ####
		// #  #
		// ####
		//    #
		//    #
		case 9:
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
				move_right();
				move_right();
			draw_fill();
				move_down();
				move_left();
				move_left();
				move_left();
				move_left();
				
			draw_fill();
			draw_fill();
			draw_fill();
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
				move_down();
				move_left();
				
			draw_fill();
			break;
	}
}
