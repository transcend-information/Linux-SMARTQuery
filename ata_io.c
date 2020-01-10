#include "ata_io.h"

/*******************************************************************************************************************/

/*basic ATA_IO function*/
/*DATA_IN*/
int ata_data_in(int sg_fd, unsigned char command, unsigned short feature, 
                     unsigned long long lba, unsigned short count, unsigned char *inqBuff, err_ret *error ){

    /*SG_IO structure*/
    sg_io_hdr_t io_hdr;
    unsigned char cdb[16];
    unsigned char sense_buffer[32]; 
    unsigned char extended = 0x00; 

    /* Prepare ATA command  data-in*/
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(&cdb, 0 ,sizeof(cdb));
    memset(&sense_buffer, 0 ,sizeof(sense_buffer));

    /*clear inqBuff*/ 
    memset(inqBuff, 0, (count * BYTES_PER_SECTOR ) );
 
    cdb[0]= 0x85; //ATA pass through    
   
    /* only support the following LBA48 command now*/
    
    if ( READ_SECTOR_EXT == command || READ_MULTIPLE_EXT == command ) {
           cdb[1]=(PIO_DATA_IN << 1) ; //PIO mode
           extended =0x01;
    }        
    else if ( READ_DMA_EXT == command) {
           cdb[1]= (DMA << 1);  //DMA mode
           extended =0x01;   
    }
    else if (READ_DMA == command){
           cdb[1]= (DMA << 1);  //DMA mode
    }
    else {
            cdb[1]= (PIO_DATA_IN  << 1) ; //default PIO mode, not extended
    }
    
    cdb[1] |= extended ;
    //end cdb[1]
     
    cdb[2]= 0x2e;
    cdb[3]= ( feature >> 8 );   //feature 15:8
    cdb[4]= feature;            //feature 7:0
    cdb[5]= ( count >> 8 );     //count 15:8
    cdb[6]= count;              //count 7:0
    cdb[7]= ( lba >> 24 );      //lba low 15:8
    cdb[8]= lba;                //lba low 7:0
    cdb[9]= ( lba >> 32 )  ;    //lba mid 15:8
    cdb[10]=( lba >> 8 )  ;     //lba mid 7:0
    cdb[11]=( lba >> 40 );      //lba high 15:8
    cdb[12]=( lba >> 16 ) ;     //lba high 7:0
    cdb[13]=( 1 << 6 ) ;        //set lba bit to 1
    cdb[14]=command;
     
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(cdb);
    /* io_hdr.iovec_count = 0; */  /* memset takes care of this */
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = ( count * BYTES_PER_SECTOR );
    io_hdr.dxferp = inqBuff;
    io_hdr.cmdp = cdb;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 120000;    /* 120000 millisecs == 120 seconds */
    /* io_hdr.flags = 0; */     /* take defaults: indirect IO, etc */
    /* io_hdr.pack_id = 0; */
    /* io_hdr.usr_ptr = NULL; */

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
     
             perror("SG_IO DATA IN error.\n");
             return -1;
    }
    
    /*set error message*/
    error->error = sense_buffer[11];
    error->count[1] = sense_buffer[12];
    error->count[0] = sense_buffer[13];
    error->lba_low[1] = sense_buffer[14];
    error->lba_low[0] = sense_buffer[15];
    error->lba_mid[1] = sense_buffer[16];       
    error->lba_mid[0] = sense_buffer[17];
    error->lba_high[1] = sense_buffer[18];
    error->lba_high[0] = sense_buffer[19];
    error->device = sense_buffer[20];
    error->status = sense_buffer[21];

    if(sense_buffer[0]!=0x72 || sense_buffer[7]!=0x0e || sense_buffer[8]!= 0x09 || 
                                       sense_buffer[9]!=0x0c || sense_buffer[10] != extended || (sense_buffer[21] & 0x01) != 0x00){
    
             fprintf(stderr, "Error Message:\n");
             fprintf(stderr, "error = %02x\n", sense_buffer[11]);
             fprintf(stderr, "status = %02x\n", sense_buffer[21]);

              return -1;    
    }

 return 0;
}//end ata_data_in

/************************************************************************************************************/

/*DATA_OUT*/
int ata_data_out(int sg_fd, unsigned char command, unsigned short feature, 
                     unsigned long long lba, unsigned short count, unsigned char *outBuff, err_ret* error){

    //SG_IO structure
    sg_io_hdr_t io_hdr;
    unsigned char cdb[16];
    unsigned char sense_buffer[32];  
    unsigned char extended = 0x00;
    
    /* Prepare ATA command  data-in*/
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    memset(&cdb, 0 ,sizeof(cdb));
    memset(&sense_buffer, 0 ,sizeof(sense_buffer));
    
    cdb[0]= 0x85; //ATA pass through    
    /* only support the following LBA48 command now*/    
    if ( WRITE_SECTOR_EXT == command || WRITE_MULTIPLE_EXT == command ) {
           cdb[1]=  (PIO_DATA_OUT << 1) ; //PIO LBA48 mode
           extended = 0x01 ;
    }        
    else if ( WRITE_DMA_EXT == command || DATA_SET_MANAGEMENT == command) {
           cdb[1]=  (DMA << 1) ;  //DMA LBA48 mode
           extended = 0x01 ;   
    }
    else if ( WRITE_DMA == command) {
           cdb[1]=  (DMA << 1) ;  //DMA mode
    }
    else {
            cdb[1]= (5 << 1) ; //default PIO mode, not extended
    }
    
    cdb[1] |= extended ;
    //end cdb[1]
     
    cdb[2]= 0x26;
    cdb[3]= ( feature >> 8 );   //feature 15:8
    cdb[4]= feature;            //feature 7:0
    cdb[5]= ( count >> 8 );     //count 15:8
    cdb[6]= count;              //count 7:0
    cdb[7]= ( lba >> 24 );      //lba low 15:8
    cdb[8]= lba;                //lba low 7:0
    cdb[9]= ( lba >> 32 )  ;    //lba mid 15:8
    cdb[10]=( lba >> 8 )  ;     //lba mid 7:0
    cdb[11]=( lba >> 40 );      //lba high 15:8
    cdb[12]=( lba >> 16 ) ;     //lba high 7:0
    cdb[13]=( 1 << 6 ) ;        //set lba bit to 1
    cdb[14]=command;
     
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(cdb);
    /* io_hdr.iovec_count = 0; */  /* memset takes care of this */
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = (count * BYTES_PER_SECTOR);
    io_hdr.dxferp = outBuff;
    io_hdr.cmdp = cdb;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 120000;    /* 120000 millisecs == 120 seconds */
    /* io_hdr.flags = 0; */     /* take defaults: indirect IO, etc */
    /* io_hdr.pack_id = 0; */
    /* io_hdr.usr_ptr = NULL; */
  
    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
   
             perror("SG_IO DATA OUT error.\n");
             return -1;
    }
    
    /*set error message*/
    error->error = sense_buffer[11];
    error->count[1] = sense_buffer[12];
    error->count[0] = sense_buffer[13];
    error->lba_low[1] = sense_buffer[14];
    error->lba_low[0] = sense_buffer[15];
    error->lba_mid[1] = sense_buffer[16];       
    error->lba_mid[0] = sense_buffer[17];
    error->lba_high[1] = sense_buffer[18];
    error->lba_high[0] = sense_buffer[19];
    error->device = sense_buffer[20];
    error->status = sense_buffer[21];

    if(sense_buffer[0]!=0x72 || sense_buffer[7]!=0x0e || sense_buffer[8]!= 0x09 || 
                                       //sense_buffer[9]!=0x0c || sense_buffer[10] != extended || (sense_buffer[21] & 0x01) != 0x00){
                                       sense_buffer[9]!=0x0c || (sense_buffer[21] & 0x01) != 0x00){
             
             fprintf(stderr, "Error Message:\n");
             fprintf(stderr, "error = %02x\n", sense_buffer[11]);
             fprintf(stderr, "status = %02x\n", sense_buffer[21]);
                     
             return -1;    
    }

 return 0;
}//end ata_data_out

/******************************************************************************************************/

/*No_DATA*/
int ata_no_data(int sg_fd, unsigned char command, unsigned short feature, 
                                  unsigned long long lba, unsigned short count, err_ret* error){

    //SG_IO structure
    sg_io_hdr_t io_hdr;
    unsigned char cdb[16];
    unsigned char sense_buffer[32];  
    unsigned char extended = 0x00;
    
    /* Prepare ATA command  data-in*/
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    memset(&cdb, 0 ,sizeof(cdb));
    memset(&sense_buffer, 0 ,sizeof(sense_buffer));
 
    /* set ext command */
    if ( READ_VERIFY_SECTOR_EXT == command || SET_MAX_ADDR_EXT == command || READ_NATIVE_MAX_ADDR_EXT ==command)
           extended = 0x01;
           
    cdb[0]=0x85; //ATA pass through    
    cdb[1]=( (NON_DATA << 1) | extended );
    cdb[2]=0x20;
    cdb[3]= ( feature >> 8 );   //feature 15:8
    cdb[4]= feature;            //feature 7:0
    cdb[5]= ( count >> 8 );     //count 15:8
    cdb[6]= count;              //count 7:0
    cdb[7]= ( lba >> 24 );      //lba low 15:8
    cdb[8]= lba;                //lba low 7:0
    cdb[9]= ( lba >> 32 )  ;    //lba mid 15:8
    cdb[10]=( lba >> 8 )  ;     //lba mid 7:0
    cdb[11]=( lba >> 40 );      //lba high 15:8
    cdb[12]=( lba >> 16 ) ;     //lba high 7:0
    cdb[13]=( 1 << 6 ) ;        //set lba bit to 1
    cdb[14]=command;
     
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(cdb);
    /* io_hdr.iovec_count = 0; */  /* memset takes care of this */
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = SG_DXFER_NONE;
    io_hdr.dxfer_len = 0;
    io_hdr.dxferp = NULL;
    io_hdr.cmdp = cdb;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 120000;    /* 120000 millisecs == 120 seconds */
    /* io_hdr.flags = 0; */     /* take defaults: indirect IO, etc */
    /* io_hdr.pack_id = 0; */
    /* io_hdr.usr_ptr = NULL; */
  
    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
    	
        perror("SG_IO NO_DATA error.\n");
        return -1;
        
    }
    
    /*set error message*/
    error->error = sense_buffer[11];
    error->count[1] = sense_buffer[12];
    error->count[0] = sense_buffer[13];
    error->lba_low[1] = sense_buffer[14];
    error->lba_low[0] = sense_buffer[15];
    error->lba_mid[1] = sense_buffer[16];       
    error->lba_mid[0] = sense_buffer[17];
    error->lba_high[1] = sense_buffer[18];
    error->lba_high[0] = sense_buffer[19];
    error->device = sense_buffer[20];
    error->status = sense_buffer[21];

    if(sense_buffer[0]!= 0x72 || sense_buffer[7] != 0x0e || sense_buffer[8]!= 0x09 || 
                                       sense_buffer[9]!=0x0c || sense_buffer[10]!=extended || (sense_buffer[21] & 0x01) != 0x00){
                                       	
              fprintf(stderr, "Error Message:\n");
              fprintf(stderr, "error = %02x\n", sense_buffer[11]);
              fprintf(stderr, "status = %02x\n", sense_buffer[21]);
                     
              return -1;    
              
    }
    
 return 0;

}//end ata_no_data


