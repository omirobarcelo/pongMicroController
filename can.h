/* can.h - Librería con las utilidades del CAN. */
#include <p30f4011.h>
#define MAX_MSG	8

// Transmite message
// DLC = msg's number of bytes
void CANSendBMsg(unsigned int id, unsigned int dlc, unsigned char *msg);
// DLC = msg's number of integer
void CANSendMsg(unsigned int id, unsigned int dlc, unsigned int *msg);
