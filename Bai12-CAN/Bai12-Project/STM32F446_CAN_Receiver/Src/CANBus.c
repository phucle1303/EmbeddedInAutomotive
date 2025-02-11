#include "CANBus.h"
#include "stm32f4xx.h"

#define CAN_AF 0x09

void CANBus_Pins_Init(void)
{
	/*Enable clock access to GPIOB*/
	RCC->AHB1ENR|=RCC_AHB1ENR_GPIOBEN;
	
	/* Configure PB8 as Pull-Up */
  GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk);  // Clear PUPD bits for PB8
  GPIOB->PUPDR |= (1U << GPIO_PUPDR_PUPD8_Pos);  // Set PULL-UP (01)

	/*Set PB8 and PB9 to alternate function*/
	GPIOB->MODER|=GPIO_MODER_MODE8_1|GPIO_MODER_MODE9_1;
	GPIOB->MODER&=~(GPIO_MODER_MODE8_0|GPIO_MODER_MODE9_0);

	/*Select which alternate function*/
	GPIOB->AFR[1]|=(CAN_AF<<GPIO_AFRH_AFSEL8_Pos)|(CAN_AF<<GPIO_AFRH_AFSEL9_Pos);

}

void CANBus_Init(void)
{


	/*Enable Clock access to CAN1*/
	RCC->APB1ENR|=RCC_APB1ENR_CAN1EN;


    /* Enter Initialization mode*/
    CAN1->MCR |= CAN_MCR_INRQ;
    /*Wait until CANBus peripheral is in initialization mdoe*/
    while ((CAN1->MSR & CAN_MSR_INAK)==0U);

    // Enable CAN1 peripheral
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    while ((CAN1->MSR & CAN_MSR_SLAK)!=0U);


    /*Configure the timing with the following parameters
     * Normal mode.
     * Loop back mode is disabled.
     * Resynchronization jump width to 1 (value - 1).
     * Prescale of 10. (value - 1)
     * Time quanta segment 1 is 2 (value - 1)
     * Time quanta segment 2 is 1 (value - 1)
     * Baud is 400Kbps
     * */

    /*Reset the non-zero initial values*/
    CAN1->BTR&=~(CAN_BTR_TS1_Msk|CAN_BTR_TS2_Msk|CAN_BTR_SJW_Msk);

    CAN1->BTR=(1<<CAN_BTR_TS1_Pos)|(0<<CAN_BTR_TS2_Pos)
    		|(9U<<CAN_BTR_BRP_Pos);

    /*Other basic parameters can be configured according your application
     * The following is the configuration:
     * Time triggered communication is disabled.
     * Automatic bus-off management is disabled.
     * Automatic wake-up is disabled.
     * Automatic retransmission is off.
     * Receive FIFO lock mode is disabled.
     * Transmit FIFO Priority is off
     * */
}


void Filter_Configuration(void)
{

	/*Filter is filter 18 and it will accept data from identifier 0x446*/

	/* Set the filter initialization mode*/
	CAN1->FMR |= CAN_FMR_FINIT;

	/*Set the slave Filter to start from 20*/
	CAN1->FMR &=~(CAN_FMR_CAN2SB_Msk);
	CAN1->FMR |=(20<<CAN_FMR_CAN2SB_Pos);

	/*Disable Filter 18*/
	CAN1->FA1R&=~(CAN_FA1R_FACT18);

	/* Set to 1 for 32-bit scale configuration*/
    CAN1->FS1R |= CAN_FS1R_FSC18;


    CAN1->FM1R &= ~CAN_FM1R_FBM18; // Set to 0 for identifier mask mode

    CAN1->sFilterRegister[18].FR1 = (0x446<<5) << 16; // Identifier

    CAN1->sFilterRegister[18].FR2 = (0x446<<5) << 16; // Identifier mask

    /*Assign filter 18 to FIFO0*/
    CAN1->FFA1R&=~CAN_FFA1R_FFA18;

    // Activate filter 18
    CAN1->FA1R |= CAN_FA1R_FACT18;


    CAN1->FMR &= ~CAN_FMR_FINIT; // Clear the filter initialization mode


}

void CANBus_Start(void)
{
    // Leave Initialization mode
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK){}

}




void CANBus_SendMessage(CANBusTxFrameDef *TXFrame )
{
    // Wait until the transmit mailbox is empty
    while ((CAN1->TSR & CAN_TSR_TME0) == 0);

    // Set the standard identifier and data length
    CAN1->sTxMailBox[0].TIR &= ~CAN_TI0R_STID;

    // Configure the transmit mailbox identifier
    CAN1->sTxMailBox[0].TIR |= (TXFrame->identifier << 21);

    // Configure data length
    CAN1->sTxMailBox[0].TDTR |= (TXFrame->length << 0);



    CAN1->sTxMailBox[0].TDLR=
    		((uint32_t)TXFrame->data[3] << CAN_TDL0R_DATA3_Pos) |
            ((uint32_t)TXFrame->data[2] << CAN_TDL0R_DATA2_Pos) |
            ((uint32_t)TXFrame->data[1] << CAN_TDL0R_DATA1_Pos) |
            ((uint32_t)TXFrame->data[0] << CAN_TDL0R_DATA0_Pos);

    CAN1->sTxMailBox[0].TDHR=
            ((uint32_t)TXFrame->data[7] << CAN_TDH0R_DATA7_Pos) |
            ((uint32_t)TXFrame->data[6] << CAN_TDH0R_DATA6_Pos) |
            ((uint32_t)TXFrame->data[5] << CAN_TDH0R_DATA5_Pos) |
            ((uint32_t)TXFrame->data[4] << CAN_TDH0R_DATA4_Pos);

    // Set the TXRQ bit to request transmission
    CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;
}

void CANBus_ReceiveMessage(CANBusRxFrameDef *RXFrame)
{
    if (CAN1->RF0R & CAN_RF0R_FMP0) { // Check if there's a pending message in FIFO0
        // Read the received identifier
        RXFrame->identifier = (CAN1->sFIFOMailBox[0].RIR >> 3) & 0x1FFFFFFF;

        // Read the data length
        RXFrame->length = CAN1->sFIFOMailBox[0].RDTR & 0x0F;

        /*Clear old data*/
        for (int i=0;i<8;i++)
        {
        	RXFrame->data[i]=0;
        }


        RXFrame->data[0]=CAN1->sFIFOMailBox[0].RDLR >>CAN_RDL0R_DATA0_Pos;
        RXFrame->data[1]=CAN1->sFIFOMailBox[0].RDLR >>CAN_RDL0R_DATA1_Pos;
        RXFrame->data[2]=CAN1->sFIFOMailBox[0].RDLR >>CAN_RDL0R_DATA2_Pos;
        RXFrame->data[3]=CAN1->sFIFOMailBox[0].RDLR >>CAN_RDL0R_DATA3_Pos;

        RXFrame->data[4]=CAN1->sFIFOMailBox[0].RDHR >>CAN_RDH0R_DATA4_Pos;
        RXFrame->data[5]=CAN1->sFIFOMailBox[0].RDHR >>CAN_RDH0R_DATA5_Pos;
        RXFrame->data[6]=CAN1->sFIFOMailBox[0].RDHR >>CAN_RDH0R_DATA6_Pos;
        RXFrame->data[7]=CAN1->sFIFOMailBox[0].RDHR >>CAN_RDH0R_DATA7_Pos;


        // Release the FIFO (not necessary for FIFO0)
        CAN1->RF0R |= CAN_RF0R_RFOM0;

        GPIOA->ODR^=GPIO_ODR_OD5;
    }
}
