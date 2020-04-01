#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include<linux/string.h>
#include <linux/slab.h>

#define PENDRIVE_VID 0x8564
#define PENDRIVE_PID 0x1000
#define BOMS_RESET                    0xFF
#define BOMS_RESET_REQTYPE           0x21
#define BOMS_GET_MAX_LUN              0xFE
#define BOMS_GET_MAX_LUN_REQTYPE     0xA1


#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])

#define READ_CAPACITY_LENGTH  0x08



// Command Block Wrapper (CBW)	
struct command_block_wrapper 
{
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

// Command Status Wrapper (CSW)
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static uint8_t cdb_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};
 



static int send_mass_storage_command(struct usb_device *device, uint8_t endpoint, uint8_t lun,uint8_t *cdb,uint8_t direction, int data_length,uint32_t tag)
{

	uint8_t cdb_len;
	int n,size;

	struct command_block_wrapper *cbw;
cbw=(struct command_block_wrapper *)kmalloc(sizeof(struct command_block_wrapper),GFP_KERNEL);


if (cdb == NULL) {
		return -1;
	}

	if (endpoint & USB_DIR_IN) {
		printk(KERN_INFO "cannot send command on IN endpoint\n");
		return -1;
	}

cdb_len = cdb_length[cdb[0]];
if ((cdb_len == 0) || (cdb_len > sizeof(cbw->CBWCB))) {
		printk(KERN_INFO "don't know how to handle this command (%02X, length %d)\n",
			cdb[0], cdb_len);
		return -1;
	}

        memset(cbw, 0, sizeof(*cbw));
	cbw->dCBWSignature[0] = 'U';
	cbw->dCBWSignature[1] = 'S';
	cbw->dCBWSignature[2] = 'B';
	cbw->dCBWSignature[3] = 'C';
        cbw->dCBWTag = tag;
	cbw->dCBWDataTransferLength = data_length;
	cbw->bmCBWFlags = direction;
	cbw->bCBWLUN = lun;
	// Subclass is 1 or 6 => cdb_len
	cbw->bCBWCBLength = cdb_len;
	memcpy(cbw->CBWCB, cdb, cdb_len);


n=usb_bulk_msg(device, usb_sndbulkpipe(device,endpoint), (void *)cbw, 31, &size, 1000);
if(n!=0)
		    printk("command transfer failed %d",n);
	    else 	
	    	printk("command sent successfully");
	
printk(KERN_INFO"sent %d CDB bytes\n", cdb_len);
printk(KERN_INFO"sent %d bytes \n",size);
	return 0;
}

static int get_mass_storage_status(struct usb_device *device, uint8_t endpoint, uint32_t expected_tag)
{	
int b,size;
struct command_status_wrapper *csw;
	csw=(struct command_status_wrapper *)kmalloc(sizeof(struct command_status_wrapper),GFP_KERNEL);
	
	b=usb_bulk_msg(device,usb_rcvbulkpipe(device,endpoint),(void*)csw,13, &size, 1000);
	if(b<0)
		printk("ERROR IN RECIVING STATUS MESG %d",b);
	if (size != 13) {
		printk("   get_mass_storage_status: received %d bytes (expected 13)\n", size);
		return -1;
	}	
	if (csw->dCSWTag != expected_tag) {
		printk("   get_mass_storage_status: mismatched tags (expected %08X, received %08X)\n",expected_tag, csw->dCSWTag);
		return -1;
	}
	
	printk(KERN_INFO "Mass Storage Status: %02X (%s)\n", csw->bCSWStatus, csw->bCSWStatus?"FAILED":"Success");
    if (csw->dCSWTag != expected_tag)
		return -1;
	if (csw->bCSWStatus) {
		// REQUEST SENSE is appropriate only if bCSWStatus is 1, meaning that the
		// command failed somehow.  Larger values (2 in particular) mean that
		// the command couldn't be understood.
		if (csw->bCSWStatus == 1)
			return -2;	// request Get Sense
		else
			return -1;
	}	

	return 0;
}


// Mass Storage device to test bulk transfers (non destructive test)
static int test_mass_storage(struct usb_device *device, uint8_t endpoint_in, uint8_t endpoint_out)
{

int r=0, s=0,size,p=0;

uint8_t *lun=(uint8_t *)kmalloc(sizeof(uint8_t),GFP_KERNEL);
uint32_t expected_tag=1;
uint32_t  max_lba, block_size;
long device_size;
uint8_t cdb[16];   	// SCSI Command Descriptor Block
//uint8_t buffer[64];

uint8_t *buffer=(uint8_t *)kmalloc(64*sizeof(uint8_t),GFP_KERNEL);
//lun=0;

 printk("Reseting usb mass storage device");
	s = usb_control_msg(device,usb_sndctrlpipe(device,0),BOMS_RESET,BOMS_RESET_REQTYPE,0,0,NULL,0,1000);
	if(s<0)
		printk("error code: %d",s);
	else
		printk("successful Reset");

printk(KERN_INFO "Reading Max LUN\n");
	r = usb_control_msg(device,usb_sndctrlpipe(device,0), BOMS_GET_MAX_LUN ,BOMS_GET_MAX_LUN_REQTYPE,
	   0, 0, (void*)lun, 1, 1000);

if (r == 0) 
{
		*lun = 0;
	} 
	printk(KERN_INFO " Max LUN = %d\n", *lun);
    

//Read Capacity

printk(KERN_INFO"Reading Capacity:\n");
	memset(buffer, 0, sizeof(*buffer));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x25;	

send_mass_storage_command(device,endpoint_out,*lun,cdb,USB_DIR_IN,READ_CAPACITY_LENGTH,expected_tag);


p=usb_bulk_msg(device,usb_rcvbulkpipe(device,endpoint_in), (void*)buffer,24, &size, 10000);
	


printk(KERN_INFO"received %d bytes\n", size);
	max_lba = be_to_int32(&buffer[0]);
	block_size = be_to_int32(&buffer[4]);
	device_size = ((long)(max_lba+1))*block_size/(1024*1024*1024);
	printk(KERN_INFO" Max LBA: %08X\n Block Size: %08X\nDevice Size: %ld GB\n", max_lba, block_size, device_size);

get_mass_storage_status(device, endpoint_in, expected_tag);


return 0;

}




static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    
       int i;
  unsigned char epAddr,epAttr; 


    struct usb_endpoint_descriptor *ep_desc;

    uint8_t endpoint_in = 0, endpoint_out = 0;

struct usb_device *device;
    device = interface_to_usbdev(interface);
  
   
    if((id->idProduct==PENDRIVE_PID) && (id->idVendor==PENDRIVE_VID))
    {
    printk(KERN_INFO "Known USB drive detected\n");
 
    }
   printk(KERN_INFO "\nReading device descriptor:\n");
   printk(KERN_INFO "VID of the known device:%x\n",id->idVendor);
   printk(KERN_INFO "PID of the known device:%x\n",id->idProduct);


   printk(KERN_INFO "USB DEVICE CLASS : %x", interface->cur_altsetting->desc.bInterfaceClass);
   printk(KERN_INFO "USB DEVICE SUB CLASS : %x", interface->cur_altsetting->desc.bInterfaceSubClass);
   printk(KERN_INFO "USB DEVICE Protocol : %x", interface->cur_altsetting->desc.bInterfaceProtocol);
   

	printk(KERN_INFO "No of interfaces: %d\n", device->config->desc.bNumInterfaces);


// Check if the device is USB attaced SCSI type Mass storage class
			if ( (interface->cur_altsetting->desc.bInterfaceClass == 0x08)
			  && (interface->cur_altsetting->desc.bInterfaceSubClass == 0x06) 
			  && (interface->cur_altsetting->desc.bInterfaceProtocol == 0x50) ) 
			{   
                         printk(KERN_INFO "valid SCSI device");
			}
	
 printk(KERN_INFO "No of Endpoints : %d\n" ,interface->cur_altsetting->desc.bNumEndpoints);

	for(i=0;i<interface->cur_altsetting->desc.bNumEndpoints;i++)
	{
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;
		epAddr = ep_desc->bEndpointAddress;
		epAttr = ep_desc->bmAttributes;
	printk(KERN_INFO"endpoint[%d].address: %02X\n", i, epAddr);


		if((epAttr & USB_ENDPOINT_XFERTYPE_MASK)==USB_ENDPOINT_XFER_BULK)
		{
			if(epAddr & 0x80)
                                {
				printk(KERN_INFO "EP %d is Bulk IN\n", i);
                                endpoint_in = ep_desc->bEndpointAddress;
                                }
			else
				{
                                printk(KERN_INFO "EP %d is Bulk OUT\n", i);
                                endpoint_out = ep_desc->bEndpointAddress;
	                        }
		}
printk(KERN_INFO "max packet size: %04X\n", ep_desc->wMaxPacketSize);
printk(KERN_INFO "polling interval: %02X\n", ep_desc->bInterval);

	}

  test_mass_storage(device,endpoint_in, endpoint_out);

       
return 0;

}
 
static void pen_disconnect(struct usb_interface *interface)
{
    printk(KERN_INFO "Pen drive removed\n");
    return;
}
 
static struct usb_device_id pen_table[] =
{
    { USB_DEVICE(PENDRIVE_VID,PENDRIVE_PID) },
    {} /* Terminating entry */
};

 
static struct usb_driver pen_driver =
{
    name :"pen_driver",//name of the device
    probe : pen_probe,// Whenever Device is plugged in
    disconnect : pen_disconnect,// When we remove a device
    id_table : pen_table, //  List of devices served by this driver
};
 
 int pen_init(void)
{
    usb_register(&pen_driver);
    printk(KERN_INFO "UAS READ Capacity Driver Inserted\n");
    return 0;
}
 
 void pen_exit(void)
{
    usb_deregister(&pen_driver);
    printk(KERN_INFO "Leaving Kernel\n");
  
}
 
module_init(pen_init);
module_exit(pen_exit);
 
MODULE_LICENSE("GPL");



