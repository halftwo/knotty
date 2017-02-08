/* $Id: crcmodel.h,v 1.3 2012/09/20 03:21:47 jiagui Exp $ */
/******************************************************************************/
/*                                                                            */
/* Author : Ross Williams (ross@guest.adelaide.edu.au.).                      */
/* Date   : 3 June 1993.                                                      */
/* Status : Public domain.                                                    */
/*                                                                            */
/* Description : This is the header (.h) file for the reference               */
/* implementation of the Rocksoft^tm Model CRC Algorithm. For more            */
/* information on the Rocksoft^tm Model CRC Algorithm, see the document       */
/* titled "A Painless Guide to CRC Error Detection Algorithms" by Ross        */
/* Williams (ross@guest.adelaide.edu.au.). This document is likely to be in   */
/* "ftp.adelaide.edu.au/pub/rocksoft".                                        */
/*                                                                            */
/* Note: Rocksoft is a trademark of Rocksoft Pty Ltd, Adelaide, Australia.    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* How to Use This Package                                                    */
/* -----------------------                                                    */
/* Step 1: Declare a variable of type cm_t. Declare another variable          */
/*         (p_cm say) of type p_cm_t and initialize it to point to the first  */
/*         variable (e.g. p_cm_t p_cm = &cm_t).                               */
/*                                                                            */
/* Step 2: Assign values to the parameter fields of the structure.            */
/*         If you don't know what to assign, see the document cited earlier.  */
/*         For example:                                                       */
/*            p_cm->cm_width = 16;                                            */
/*            p_cm->cm_poly  = 0x8005L;                                       */
/*            p_cm->cm_init  = 0L;                                            */
/*            p_cm->cm_refin = TRUE;                                          */
/*            p_cm->cm_refout = TRUE;                                          */
/*            p_cm->cm_xorout = 0L;                                            */
/*         Note: Poly is specified without its top bit (18005 becomes 8005).  */
/*         Note: Width is one bit less than the raw poly width.               */
/*                                                                            */
/* Step 3: Initialize the instance with a call cm_ini(p_cm);                  */
/*                                                                            */
/* Step 4: Process zero or more message bytes by placing zero or more         */
/*         successive calls to cm_nxt. Example: cm_nxt(p_cm,ch);              */
/*                                                                            */
/* Step 5: Extract the CRC value at any time by calling crc = cm_crc(p_cm);   */
/*         If the CRC is a 16-bit value, it will be in the bottom 16 bits.    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Design Notes                                                               */
/* ------------                                                               */
/* PORTABILITY: This package has been coded very conservatively so that       */
/* it will run on as many machines as possible. For example, all external     */
/* identifiers have been restricted to 6 characters and all internal ones to  */
/* 8 characters. The prefix cm (for Crc Model) is used as an attempt to avoid */
/* namespace collisions. This package is endian independent.                  */
/*                                                                            */
/* EFFICIENCY: This package (and its interface) is not designed for           */
/* speed. The purpose of this package is to act as a well-defined reference   */
/* model for the specification of CRC algorithms. If you want speed, cook up  */
/* a specific table-driven implementation as described in the document cited  */
/* above. This package is designed for validation only; if you have found or  */
/* implemented a CRC algorithm and wish to describe it as a set of parameters */
/* to the Rocksoft^tm Model CRC Algorithm, your CRC algorithm implementation  */
/* should behave identically to this package under those parameters.          */
/*                                                                            */
/******************************************************************************/

#ifndef CRCMODEL_H_
#define CRCMODEL_H_ 1

typedef unsigned long  ulong;
typedef unsigned       bool;
typedef unsigned char  ubyte;

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif


/* CRC Model Abstract Type 
 * The following type stores the context of an executing instance of the 
 * model algorithm. Most of the fields are model parameters which must be
 * set before the first initializing call to cm_ini.
 */
typedef struct
{
	int   cm_width;		/* Param: Width in bits [8,32].       */
	ulong cm_poly; 		/* Param: The algorithm's polynomial. */
	ulong cm_init; 		/* Param: Initial register value.     */
	bool  cm_refin;		/* Param: Reflect input bytes?        */
	bool  cm_refout;	/* Param: Reflect output CRC?         */
	ulong cm_xorout;	/* Param: XOR this to output CRC.     */

	ulong cm_reg;		/* Context: Context during execution. */
} cm_t;


/* Initializes the argument CRC model instance. 
 * All parameter fields must be set before calling this.
 */
void cm_ini(cm_t *p_cm);


/* Processes a single message byte [0,255].  */
void cm_nxt(cm_t *p_cm, int ch);


/* Processes a block of message bytes.  */
void cm_blk(cm_t *p_cm, ubyte *blk_adr, ulong blk_len);


/* Returns the CRC value for the message bytes processed so far.  */
ulong cm_crc(cm_t *p_cm);


/* Functions For Table Calculation
 * The following function can be used to calculate a CRC lookup table.
 * It can also be used at run-time to create or check static tables.
 *
 * Returns the i'th entry for the lookup table for the specified algorithm.
 * The function examines the fields cm_width, cm_poly, cm_refin, and the
 * argument table index in the range [0,255] and returns the table entry in
 * the bottom cm_width bytes of the return value.
 */
ulong cm_tab(cm_t *p_cm, int index);

#endif

