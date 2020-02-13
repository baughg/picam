#include <stdio.h>
#include <bcm2835.h>
#include <stdint.h>

#define CSW	RPI_GPIO_P1_12 
#define CPWR	RPI_GPIO_P1_11 
#define PWR	RPI_GPIO_P1_15 

int main(int argc, char** argv)
{
    if(!bcm2835_init())
    {
	printf("Can not initialise GPIO!\n");
	return -1;
    }
    
    printf("Canon camera power controller.\n");
    
    bcm2835_gpio_fsel(CPWR, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(CSW, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PWR, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(PWR, BCM2835_GPIO_PUD_UP);
    uint8_t pwr_state = 0;

    if(argc >= 2)
    {
	printf("switching off camera...\n");
    	bcm2835_gpio_write(CSW,LOW);
    	bcm2835_delay(5000);
    	bcm2835_gpio_write(CSW,HIGH);	
	exit(0);
    }
    bcm2835_delay(1000);
    printf("Powering down Camera for 5 secs.\n");
    bcm2835_gpio_write(CPWR,LOW);
    bcm2835_gpio_write(CSW,LOW);
    bcm2835_delay(5000);
    bcm2835_gpio_write(CPWR,HIGH);
    printf("12V camera supply on...\n");
    bcm2835_delay(5000);
    bcm2835_gpio_write(CSW,HIGH);
    printf("Engaging power button...\n");
    bcm2835_delay(2000);
    bcm2835_gpio_write(CSW,LOW);
    printf("Camera power sequence complete.\n");
    exit(0);
	while(1)
	{
	}
    while(1) {	
	pwr_state = bcm2835_gpio_lev(PWR);

	if(!pwr_state)
	{
	    printf("Power");
	}
    	//bcm2835_gpio_write(VENT,HIGH);
	//bcm2835_delay(5000);
	//bcm2835_delay(5000);
    	//bcm2835_gpio_write(VENT,LOW);
	//	bcm2835_delay(5000);

    }
    return 0;
}
