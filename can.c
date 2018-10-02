/* can.c - Implementación de las funciones de can.h. */
#include "can.h"

void CANSendBMsg(unsigned int id, unsigned int dlc, unsigned char *msg) {
	
	// Standard Id
	C1TX0SIDbits.SID5_0 = id;
	id = id >> 6;
	C1TX0SIDbits.SID10_6 = id;

	// RTR
	C1TX0DLCbits.TXRTR = 0;		// Normal message

	// IDE
	C1TX0SIDbits.TXIDE = 0;		// Standard identifier

	// DLC
	C1TX0DLCbits.DLC = dlc;		// Data Length Code

	// Message
	unsigned int aux;
	switch (dlc) {
		case 1:
			C1TX0B1 = msg[0];
			break;
		case 2:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			break;
		case 3:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			C1TX0B2 = msg[2];
			break;
		case 4:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			aux = msg[3];
			aux = aux << 8;
			C1TX0B2 = aux | msg[2];
			break;
		case 5:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			aux = msg[3];
			aux = aux << 8;
			C1TX0B2 = aux | msg[2];
			C1TX0B3 = msg[4];
			break;
		case 6:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			aux = msg[3];
			aux = aux << 8;
			C1TX0B2 = aux | msg[2];
			aux = msg[5];
			aux = aux << 8;
			C1TX0B3 = aux | msg[4];
			break;
		case 7:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			aux = msg[3];
			aux = aux << 8;
			C1TX0B2 = aux | msg[2];
			aux = msg[5];
			aux = aux << 8;
			C1TX0B3 = aux | msg[4];
			C1TX0B4 = msg[6];
			break;
		case 8:
			aux = msg[1];
			aux = aux << 8;
			C1TX0B1 = aux | msg[0];
			aux = msg[3];
			aux = aux << 8;
			C1TX0B2 = aux | msg[2];
			aux = msg[5];
			aux = aux << 8;
			C1TX0B3 = aux | msg[4];
			aux = msg[7];
			aux = aux << 8;
			C1TX0B4 = aux | msg[6];
			break;
	}

	C1TX0CONbits.TXREQ = 1;		// Send message
}

void CANSendMsg(unsigned int id, unsigned int dlc, unsigned int *msg) {
	// Standard Id
	C1TX0SIDbits.SID5_0 = id;
	id = id >> 6;
	C1TX0SIDbits.SID10_6 = id;

	// RTR
	C1TX0DLCbits.TXRTR = 0;		// Normal message

	// IDE
	C1TX0SIDbits.TXIDE = 0;		// Standard identifier

	// DLC
	C1TX0DLCbits.DLC = dlc*2;	// Data Length Code
	
	int aux;
	if (dlc >= 1) {
		aux = msg[0];
		C1TX0B1 = aux;
	}
	if (dlc >= 2) {
		aux = msg[1];
		C1TX0B2 = aux;
	}
	if (dlc >= 3) {
		aux = msg[2];
		C1TX0B3 = aux;
	}
	if (dlc >= 4) {
		aux = msg[3];
		C1TX0B4 = aux;
	}
	
	C1TX0CONbits.TXREQ = 1;				// Send message
	while (C1TX0CONbits.TXREQ == 1);	// Wait until successfully transmitted
}
