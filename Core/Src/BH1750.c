#include "BH1750.h"

typedef   unsigned char BYTE;
//BYTE    BUF[8];                         //接收数据缓存区  

void HAL_Delay_us(uint32_t us)
{
    uint32_t loop = us * (SystemCoreClock / 1000000UL);
    while(loop--);
}

void MyI2C_W_SCL(uint8_t BitValue)
{
	HAL_GPIO_WritePin(GPIOB, BH1750_SCL_GPIO_PIN, (BitValue) ? GPIO_PIN_SET : GPIO_PIN_RESET);		//根据BitValue，设置SCL引脚的电平
	HAL_Delay_us(10);												//延时10us，防止时序频率超过要求
}
 
void MyI2C_W_SDA(uint8_t BitValue)
{
	HAL_GPIO_WritePin(GPIOB, BH1750_SDA_GPIO_PIN, (BitValue)? GPIO_PIN_SET : GPIO_PIN_RESET);		//根据BitValue，设置SDA引脚的电平，BitValue要实现非0即1的特性
	HAL_Delay_us(10);												//延时10us，防止时序频率超过要求
}

uint8_t MyI2C_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = HAL_GPIO_ReadPin(GPIOB, BH1750_SDA_GPIO_PIN);		//读取SDA电平
	HAL_Delay_us(10);												//延时10us，防止时序频率超过要求
	return BitValue;											//返回SDA电平
}

//iic接口初始化
 
/**
**  设置SDA为输出
**/
void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructer;
    GPIO_InitStructer.Pin= BH1750_SDA_GPIO_PIN;
    GPIO_InitStructer.Speed=GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructer.Mode=GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(BH1750_GPIO_PORT, &GPIO_InitStructer);
}
 
 
/**
**  设置SDA为输入
**/
void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStructer;
    GPIO_InitStructer.Pin= BH1750_SDA_GPIO_PIN;
    GPIO_InitStructer.Speed=GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructer.Mode=GPIO_MODE_INPUT;
    HAL_GPIO_Init(BH1750_GPIO_PORT, &GPIO_InitStructer);
}
 
 
void Single_Write_BH1750(uchar REG_Address)
{
   BH1750_IIC_Start();
   BH1750_IIC_Send_Byte(BHAddWrite);
   BH1750_IIC_Send_Byte(REG_Address);
   BH1750_IIC_Stop();
}
 
void BH1750_Init(void)
{
	Single_Write_BH1750(0x01);
	bh_data_send(BHPowOn);   // BH1750上电
	bh_data_send(BHReset);   // BH1750复位
	bh_data_send(BHModeH2);  // BH1750高分辨率模式2: 0.5lx, 120ms
}
 
//产生IIC起始信号
void BH1750_IIC_Start(void)
{
	MyI2C_W_SDA(1);	  	  
	MyI2C_W_SCL(1);
 	MyI2C_W_SDA(0);//START:when CLK is high,DATA change form high to low 
	MyI2C_W_SCL(0);//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void BH1750_IIC_Stop(void)
{
	SDA_OUT();//sda线输出
	MyI2C_W_SCL(0);
	MyI2C_W_SDA(0);//STOP:when CLK is high DATA change form low to high
 	HAL_Delay_us(4);
	MyI2C_W_SCL(1); 
	MyI2C_W_SDA(1);//发送I2C总线结束信号
	HAL_Delay_us(4);							   	
}
//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
uint8_t BH1750_IIC_Wait_Ack(void)
{
	uint8_t ucErrTime=0;
	SDA_IN();      //SDA设置为输入  
	MyI2C_W_SDA(1);
	HAL_Delay_us(1);	   
	MyI2C_W_SCL(1);
	HAL_Delay_us(1);	 
	while(MyI2C_R_SDA())
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			BH1750_IIC_Stop();
			return 1;
		}
	}
	MyI2C_W_SCL(0);//时钟输出0 	   
	return 0;  
} 
//产生ACK应答
void BH1750_IIC_Ack(void)
{
	MyI2C_W_SCL(0);
	SDA_OUT();
	MyI2C_W_SDA(0);
	HAL_Delay_us(2);
	MyI2C_W_SCL(1);
	HAL_Delay_us(2);
	MyI2C_W_SCL(0);
}
//不产生ACK应答		    
void BH1750_IIC_NAck(void)
{
	MyI2C_W_SCL(0);
	SDA_OUT();
	MyI2C_W_SDA(1);
	HAL_Delay_us(2);
	MyI2C_W_SCL(1);
	HAL_Delay_us(2);
	MyI2C_W_SCL(0);
}					 				     
//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
void BH1750_IIC_Send_Byte(uint8_t txd)
{                        
    uint8_t t;   
	SDA_OUT(); 	    
    MyI2C_W_SCL(0);//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        //IIC_SDA=(txd&0x80)>>7;
			if((txd&0x80)>>7)
				MyI2C_W_SDA(1);
			else
				MyI2C_W_SDA(0);
				txd<<=1; 	  
				HAL_Delay_us(2);   //对TEA5767这三个延时都是必须的
				MyI2C_W_SCL(1);
				HAL_Delay_us(2); 
				MyI2C_W_SCL(0);	
				HAL_Delay_us(2);
    }	 
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
uint8_t BH1750_IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	SDA_IN();//SDA设置为输入
  for(i=0;i<8;i++ )
	{
		MyI2C_W_SCL(0); 
		HAL_Delay_us(2);
		MyI2C_W_SCL(1);
		receive<<=1;
		if(MyI2C_R_SDA())
			receive++;   
		HAL_Delay_us(1); 
   }					 
    if (!ack)
        BH1750_IIC_NAck();//发送nACK
    else
        BH1750_IIC_Ack(); //发送ACK   
    return receive;
}
 
/*************************************************************************************/
void bh_data_send(uint8_t command)
{
    
    BH1750_IIC_Start();                      //iic起始信号
    BH1750_IIC_Send_Byte(BHAddWrite);       //发送器件地址
    while(BH1750_IIC_Wait_Ack());           //等待从机应答
    BH1750_IIC_Send_Byte(command);          //发送指令
    BH1750_IIC_Wait_Ack();                   //等待从机应答
    BH1750_IIC_Stop();                       //iic停止信号
}
 
uint16_t bh_data_read(void)
{
	uint16_t buf;
	BH1750_IIC_Start();                       //iic起始信号
	BH1750_IIC_Send_Byte(BHAddRead);         //发送器件地址+读标志位
	BH1750_IIC_Wait_Ack();                     //等待从机应答
	buf=BH1750_IIC_Read_Byte(1);              //读取数据
	buf=buf<<8;                        //读取并保存高八位数据
	buf+=0x00ff&BH1750_IIC_Read_Byte(0);      //读取并保存第八位数据
	BH1750_IIC_Stop();                        //发送停止信号 
	return buf; 
}
