/*
 * types.h
 *
 * Types definitions
 *
 * Header GPL 
 * -------------------------------------------------------------------------- */

#ifndef HIDTYPES_H
#define HIDTYPES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/types.h>
	
/* 
 * Constants
 * -------------------------------------------------------------------------- */
#define PATH_SIZE			10		/* Deep max for Path							*/
#define USAGE_TAB_SIZE	50		/* Size of usage stack							*/
#define MAX_REPORT		300		/* Including FEATURE, INPUT and OUTPUT	*/
#define REPORT_DSC_SIZE	6144	/* Size max of Report Descriptor				*/
#define MAX_REPORT_TS	3		/* Max time validity of a report				*/

/* 
 * Items
 * -------------------------------------------------------------------------- */
#define SIZE_0						0x00
#define SIZE_1						0x01
#define SIZE_2						0x02
#define SIZE_4						0x03
#define SIZE_MASK					0x03
                              
#define TYPE_MAIN					0x00
#define TYPE_GLOBAL				0x04
#define TYPE_LOCAL				0x08
#define TYPE_MASK					0x0C

/* Main items */
#define ITEM_COLLECTION			0xA0
#define ITEM_END_COLLECTION		0xC0
#define ITEM_FEATURE				0xB0
#define ITEM_INPUT				0x80
#define ITEM_OUTPUT				0x90

/* Global items */
#define ITEM_UPAGE				0x04
#define ITEM_LOG_MIN				0x14
#define ITEM_LOG_MAX				0x24
#define ITEM_PHY_MIN				0x34
#define ITEM_PHY_MAX				0x44
#define ITEM_UNIT_EXP				0x54
#define ITEM_UNIT					0x64
#define ITEM_REP_SIZE				0x74
#define ITEM_REP_ID				0x84
#define ITEM_REP_COUNT			0x94

/* Local items */
#define ITEM_USAGE				0x08
#define ITEM_STRING				0x78

/* Long item */
#define ITEM_LONG                               0xFC

#define ITEM_MASK					0xFC

/* Attribut Flags */
#define ATTR_DATA_CST			0x01
#define ATTR_NVOL_VOL			0x80

/* 
 * HIDNode struct
 *
 * Describe a HID Path point 
 * -------------------------------------------------------------------------- */
typedef struct
{
	u_short UPage;
	u_short Usage;
} HIDNode;

/* 
 * HIDPath struct
 *
 * Describe a HID Path
 * -------------------------------------------------------------------------- */
typedef struct
{
	u_char   Size;					/* HID Path size	*/
	HIDNode Node[PATH_SIZE];		/* HID Path		*/
} HIDPath;

/* 
 * HIDData struct
 *
 * Describe a HID Data with its location in report 
 * -------------------------------------------------------------------------- */
typedef struct
{
	long    Value;		/* HID Object Value						*/
	HIDPath Path;		/* HID Path								*/

	u_char   ReportID;	/* Report ID								*/
	u_char   Offset;		/* Offset of data in report					*/
	u_char   Size;		/* Size of data in bit						*/
                            
	u_char   Type;		/* Type : FEATURE / INPUT / OUTPUT	*/
	u_char   Attribute;	/* Report field attribute					*/
                            
	long   Unit;			/* HID Unit								*/
	char    UnitExp;		/* Unit exponent*/
                                                                          
	long    LogMin;		/* Logical Min							*/
	long    LogMax;		/* Logical Max							*/
	long    PhyMin;		/* Physical Min							*/
	long    PhyMax;		/* Physical Max							*/
} HIDData;

/*
 * HIDParser struct
 * -------------------------------------------------------------------------- */
typedef struct
{
	u_char   ReportDesc[REPORT_DSC_SIZE];	/* Store Report Descriptor					*/
	u_short  ReportDescSize;					/* Size of Report Descriptor					*/
	u_short  Pos;							/* Store current pos in descriptor				*/
	u_char   Item;							/* Store current Item							*/
	long    Value;							/* Store current Value						*/

	HIDData Data;							/* Store current environment					*/

	u_char   OffsetTab[MAX_REPORT][4];		/* Store ID, Type, offset & timestamp of report	*/
	u_char   ReportCount;					/* Store Report Count						*/
	u_char   Count;							/* Store local report count					*/

	u_short  UPage;							/* Global UPage								*/
	HIDNode UsageTab[USAGE_TAB_SIZE];	/* Usage stack								*/
	u_char   UsageSize;						/* Design number of usage used				*/

	u_char   nObject;						/* Count Objects in Report Descriptor			*/
	u_char   nReport;						/* Count Reports in Report Descriptor			*/
} HIDParser;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* HIDTYPES_H */
