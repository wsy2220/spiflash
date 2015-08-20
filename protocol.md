#Packet format

ACK, STX, SOH, ETX and NAK are ASCII control characters.

Inum and Onum are in little endian.
##Master:

Type:	SOH		Inum	Onum	STX		DATA	...		ETX

No:		0		1-2		3-4		5		6		...		Inum+6

##Programmer:

##On header correct

Type:	ACK

No:		0

##On success

Type:	ACK		STX		DATA	...		ETX

No:		0		1		2		...		Onum+2

##On Fail

Type:	NAK

No:		0

NAK will be sent at any time when the programmer detects an error.


#Operation

The master sends the packet header (including Inum and Onum) and wait for ACK,
then sends the remaining part of the packet

After that the programmer will return a full packet, or NAK on error 

