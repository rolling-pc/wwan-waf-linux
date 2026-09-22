#include "crc.h"

/*********************************************************************/

unsigned int crc_16_l_calc(char* buf_ptr,unsigned int len)
{
    unsigned int i;
    unsigned short crc = 0;
    
    while (len--!=0)
    {
        for(i = 0x80; i !=0 ; i = i>>1)
        {
            if((crc & 0x8000) !=0 )
            {
                crc = (unsigned short)(crc << 1) ;
                crc = (unsigned short)(crc ^ 0x1021);
            }
            else
            {
                crc = (unsigned short)(crc << 1) ;
            } 
            
            if((*buf_ptr & i) != 0 )
            {
                crc = (unsigned short)(crc ^ 0x1021);
            }
        }
        buf_ptr++;
    }
	
	return (crc);

}

unsigned short frm_chk( unsigned short *src, int len,int nEndian )
{
    unsigned int sum = 0;
    unsigned short SourceValue, DestValue;
    unsigned short lowSourceValue, hiSourceValue;

    /* Get sum value of the source.*/
    while (len > 1)
    {     
        SourceValue = *src++;
        if( nEndian == 1 )
		{
			// Big endian
			DestValue   = 0;      
			lowSourceValue = (unsigned short)(( SourceValue & 0xFF00 ) >> 8);
			hiSourceValue = (unsigned short)(( SourceValue & 0x00FF ) << 8);
			DestValue = (unsigned short)(lowSourceValue | hiSourceValue);
		}
		else
		{
			// Little endian
			DestValue = SourceValue;
		}
        sum += DestValue;	
        len -= 2;
    }

    if (len == 1)
    {
        sum += *( (unsigned char *) src );
    }

    sum = (sum >> 16) + (sum & 0x0FFFF);
    sum += (sum >> 16);

    return (unsigned short)(~sum);   
}
