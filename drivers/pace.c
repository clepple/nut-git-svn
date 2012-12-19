

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"PACE NUT driver"
#define DRIVER_VERSION	"1.06"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	DRV_STABLE,
	{ NULL }
};

#define ENDCHAR  13	/* replies end with CR */
#define MAXTRIES 5
#define UPSDELAY 50000	/* 50 ms delay required for reliable operation */

#define SER_WAIT_SEC	3	/* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC	0
#define TRUE 1
#define FALSE 0

char buf[63];

char *command = "AD00";
enum status_position
		    {
		      ERR_STATUS              = 56,
		      CHARGE_SOURCE           = 58,
		      INV_STATUS              = 59,
		      BAT_STATUS              = 60,
		      LOAD_STATUS             = 61,
		      BAT_CHRG_DISCHRG_STATUS = 63
		    };
enum err_length
		    {
		        ERR_LEN  = 2,
			CHRGE_LEN =1,
			INV_LEN   =1,
			BAT_LEN   =1,
			LOAD_LEN  =2,
			CHRG_DCHRG_LEN =1
		    };
void upsdrv_initinfo(void)
{
	
	dstate_setinfo("ups.mfr", "%s", "PACE");
	dstate_setinfo("ups.model", "PACE  %s", "UPS7");
	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"), 
	dstate_getinfo("ups.model"), device_path);
	
}


void upsdrv_shutdown(void)
{
/*	printf("The UPS will shut down in approximately one minute.\n");

	if (ups_on_line())
		printf("The UPS will restart in about one minute.\n");
	else
		printf("The UPS will restart when power returns.\n");

	ser_send_pace(upsfd, UPSDELAY, "S01R0001\r");
*/
}
void test_EOL()
{
  char ch;
  int y,test = 1;
  while(test){
	do
	{
	
	    y = ser_get_char(upsfd,&ch, SER_WAIT_SEC,SER_WAIT_USEC);
	
	}
	while(ch!='\n');
	y = ser_get_char(upsfd,&ch, SER_WAIT_SEC,SER_WAIT_USEC);
	if(ch != '\r')
	    test =1;
	else
	  test =0;
  }	
	  return;
}
int test_CMD()
{ 
  int i,r;
  char header[4];
      for (i=0;i<4;i++){
         header[i]=buf[i];
      }
      header[i] = '\0';
      r = strcmp(header,command);
      if(r == 0)
	return TRUE;
      else
	return FALSE;
}

void fill_buffer()
{
	int x;
	test_EOL();
	
    	x= ser_get_buf_len(upsfd,buf, 64, SER_WAIT_SEC,SER_WAIT_USEC);
	printf("reading status is %d\n",x);
	printf("the reading data is %s\n",buf);
}

void update_err_status()
{
      int i;
      int x;
      char data[2];
      for(i=0;i<ERR_LEN;i++)
      {
	 data[i] = buf[ERR_STATUS+i];
      }
      data[i] = '\0';
      x = atoi(data);
      if(x==0)
	dstate_setinfo("error.status", "%d",0);        //No error
      else
	dstate_setinfo("error.status", "%d",x);        //update error code
}

void update_charge_source_status()
{
      if( (buf[CHARGE_SOURCE]) == '0'){
	dstate_setinfo("charge.solar", "%s","OFF");   //charge on mains
      }
      else
	dstate_setinfo("charge.solar", "%s","ON");    //charge on solar
}

void update_inverter_status()
{
  if((buf[INV_STATUS]) == '0')
    dstate_setinfo("inverter.status", "%s","OFF");
  else
    dstate_setinfo("inverter.status", "%s","ON");
}

void update_battery_status()
{
  if((buf[BAT_STATUS])=='0')
    status_set("OB");                   //battery normal
  else if((buf[BAT_STATUS])=='1')
    status_set("RB");                  //battery low trip output
  else
    status_set("LB");
}

void update_load_status()
{
      int i;
      int x;
      char data[2];
      for(i=0;i<LOAD_LEN;i++)
      {
	 data[i] = buf[LOAD_STATUS+i];
      }
      data[i] = '\0';
      x = atoi(data);
      if(x == 2)
	status_set("OVER");             //over load
      else
	status_set("TRIM");             //over load trip out
}

void update_battery_charge_dchrg_status()
{
  if (buf[BAT_CHRG_DISCHRG_STATUS] == '0')
    status_set("DISCHRG");
  else
    status_set("CHRG");
}

void upsdrv_updateinfo(void)
{
	int r;
	int x,y,i=0,j,k,value;
	float value1;
	
	char *test = "1234";
	char *ptr[]= {"input.voltage","output.voltage","output.current","battery.voltage","battery.current",
			      "pannel.voltage","pannel.current","input.frequency","output.frequency",
			      "ups.load","pannel.kw","pannel.kwh"
			     };
	char ch;
	char data[10];
	int index[] = {4,4,4,4,4,4,4,4,4,4,4,8};
	int divident [] = {10,10,100,10,100,10,100,10,10,10,10,100};
	int data_position = 4;
	
	test_EOL();
	
    	x= ser_get_buf_len(upsfd,buf, 64, SER_WAIT_SEC,SER_WAIT_USEC);
	printf("reading status is %d\n",x);
	printf("the reading data is %s\n",buf);

	ser_comm_good();
	r = test_CMD();
	if(r != TRUE){
	  printf("command error\n");
	  return;
	}
	i = data_position;
	for(j =0;j<12;j++)
	{
	   
		 for(k=0;k<index[j];k++)
		 {
			data[k] = buf[i+k];
			
		
		 }
		 data[k] = '\0';
		 printf("the reading data is %s\n",data);
		 value = atoi(data);
		 printf("the interger value %d\n",value);
		 value1 = (float)value/divident[j];
		 printf("value 1 = %f\n",value1);
		 dstate_setinfo(ptr[j], "%0.2f",value1);
		 i = i+index[j];
	    
	}
	status_init();
	update_err_status();
	update_charge_source_status();
	update_inverter_status();
	update_battery_status();
	update_load_status();
	update_battery_charge_dchrg_status();
	status_commit();
	dstate_dataok();
  
	
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "pannel.voltage", "Override nominal battery voltage");
	addvar(VAR_VALUE, "pannel.current", "Battery voltage multiplier");
	addvar(VAR_VALUE, "pannel.kw", "Override nominal battery voltage");
	addvar(VAR_VALUE, "pannel.kwh", "Battery voltage multiplier");
	addvar(VAR_VALUE, "inverter.status", "Battery voltage multiplier");
	addvar(VAR_VALUE, "charge.solar", "Battery voltage multiplier");
	addvar(VAR_VALUE, "error.status", "Battery voltage multiplier");
}

void upsdrv_initups(void)
{
	int fdm, fds;
	char *slavename;                       //only for ptmx
	extern char *ptsname();                //only for ptmx
     
	upsfd = ser_open(device_path);
	grantpt(upsfd);                        /* change permission of slave , only for ptmx*/
	unlockpt(upsfd);                       /* unlock slave ,only for ptmx */
	slavename = ptsname(upsfd);           //only for ptmx
	printf("the slave name is %s\n",slavename);
	//upsfd = ser_open(device_path);
	printf("driver initups is running");
	ser_set_speed(upsfd, device_path, B38400);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
