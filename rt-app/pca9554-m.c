/*******************************************************************
 * pca9554.c - HEIG-VD 2008, Cours IEM
 *
 * Author: DRE
 * Date: December 2008
 *******************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>


#include "pca9554-m.h"
#include "xeno-i2c.h"

#define MAJOR_NUM			52

#define MINOR_NUM_LED0		0
#define MINOR_NUM_LED1		1
#define MINOR_NUM_LED2		2
#define MINOR_NUM_LED3		3
#define MINOR_NUM_SW0		4
#define MINOR_NUM_SW1		5
#define MINOR_NUM_SW2		6
#define MINOR_NUM_SW3		7

#define MINOR_COUNT			8

// Fonctions prototype pour les accès USER
ssize_t pca9554_write(struct file *, const char __user *, size_t, loff_t *);
ssize_t pca9554_read(struct file *, char __user *, size_t, loff_t *);

// Exportations
EXPORT_SYMBOL(pca9554_en_led);
EXPORT_SYMBOL(pca9554_dis_led);
EXPORT_SYMBOL(pca9554_get_switch);
EXPORT_SYMBOL(pca9554_send);
EXPORT_SYMBOL(pca9554_receive);

dev_t dev;

static uint8_t mask_led = 0;
static uint8_t mask_switch = 0;
static uint8_t dummy_data;

#define I2C_SLAVE 0x0703 			/* IOCTL CMD value to be passed to xeno_i2c_ioctl */

struct file_operations fops = {
	.read = pca9554_read,
	.write = pca9554_write
};

struct cdev *my_dev;

ssize_t pca9554_en_led(uint8_t led_num){
	if(led_num >= MINOR_NUM_LED0 && led_num <= MINOR_NUM_LED3){
		mask_led |= (1 << led_num);
		return 0;
	}
	return -1;
}

ssize_t pca9954_dis_led(uint8_t led_num){
	if(led_num >= MINOR_NUM_LED0 && led_num <= MINOR_NUM_LED3){
		mask_led &= ~(1 << led_num);
		return 0;
	}
	return -1;
}

ssize_t pca9554_get_switch(uint8_t switch_num, uint8_t *switch_val){
	if(switch_num >= MINOR_NUM_SW0 && switch_num <= MINOR_NUM_SW3){
		switch_val = (mask_switch >> switch_num) & 0x01;
		return 0;
	}
	return -1;
}

ssize_t pca9554_send(){
	struct file tmp_file;
	tmp_file.private_data = (void *)dummy_data;
	return pca9554_write(&tmp_file, NULL, 0, NULL);
}

ssize_t pca9554_receive(){
	struct file tmp_file;
	tmp_file.private_data = (void *)dummy_data;
	return pca9554_read(&tmp_file, NULL, 0, NULL);
}

ssize_t pca9554_read(struct file *file, char __user *buff, size_t len, loff_t *off) {
	char datas[1];

	if(file->private_data == NULL){
		// Accès depuis le USER SPACE
		printk(KERN_ERR "PCA9554: access form USER SPACE is prohibited\n");
		return -1;
	}
	// Accès depuis le KERNEL SPACE

	// Récupèration des données
	datas[0] = 0;				// Input register
	xeno_i2c_write(datas, 1);	// On écrit la valeur de registre
	xeno_i2c_read(datas, 1);	// On lit le résultat

	// Traitement des données
	mask_switch = ((~datas[0]) >> 4) & 0x0F;
	return 1;
}

ssize_t pca9554_write(struct file *file, const char __user *buff, size_t len, loff_t *off) {
	char datas[2];

	if(file->private_data == NULL){
		// Accès depuis le USER SPACE
		printk(KERN_ERR "PCA9554: access form USER SPACE is prohibited\n");
		return -1;
	}

	// Accès depuis le KERNEL SPACE

	/**
	 * On écrit nos données dans le PCA9554
	 */
	datas[0] = 1;				// Output register
	datas[1] = ~mask_led & 0x0F;
	xeno_i2c_write(datas, 2);	// On écrit la valeur de registre

	return 1;
}

/*! \brief Point d'entrée du module pca9554
 *  Fonction appelée lorsque l'on insère le module dans le noyau
 */
int __init init_module(void) {
	char datas[2];	// Donnée à envoyé
	dev_t dev;
	// Initialisation de l'i2c
	if(xeno_i2c_init() == 0){
		printk(KERN_INFO "PCA9554: i2c init success\n");
	}else{
		printk(KERN_ERR "PCA9554: i2c init error\n");
		return -1;
	}
	// Configuration de l'adresse de l'esclave
	if(xeno_i2c_ioctl(I2C_SLAVE, 0x0020) == 0){
		printk(KERN_INFO "PCA9554: i2c configuration slave address success\n");
	}else{
		printk(KERN_ERR "PCA9554: i2c configuration slave address error\n");
		return -1;
	}

	// On configure le PCA9554 pour avoir les 4 switch en entrée et les 4 leds en sortie

	// On définit le sens des I/Os (entrée ou sortie)
	datas[0] = 3;		// Configuration register
	datas[1] = 0xF0;	// Valeurs
	if(xeno_i2c_write(datas, 2) == 0){
		printk(KERN_INFO "PCA9554: i2c setup I/Os success\n");
	}else{
		printk(KERN_ERR "PCA9554: i2c setup I/Os error\n");
		return -1;
	}

	// On inverse la polarité pour les leds
	datas[0] = 2;		// Polarity register
	datas[1] = 0x00;	// Valeurs
	if(xeno_i2c_write(datas, 2) == 0){
		printk(KERN_INFO "PCA9554: i2c setup polarity success\n");
	}else{
		printk(KERN_ERR "PCA9554: i2c setup polarity error\n");
		return -1;
	}

	datas[0] = 1;		// Output register
	datas[1] = 0x0F;	// On eteind les leds
	xeno_i2c_write(datas, 2);

	// Création d'un noeud pour le module avec c 52 0
	dev = MKDEV(MAJOR_NUM, 0);

	my_dev = cdev_alloc();
	cdev_init(my_dev, &fops);

	my_dev->owner = THIS_MODULE;
	cdev_add(my_dev, dev, 8);
	printk(KERN_INFO "PCA9554: module insertion terminated\n");
	return 0;
}

/*! \brief Point de sortie du module pca9554
 *  Fonction appelée lorsque l'on ressort le module du noyau
 */
void __exit cleanup_module(void) {
	char datas[2];
	datas[0] = 1;		// Output register
	datas[1] = 0x0F;	// On eteind les leds
	xeno_i2c_write(datas, 2);

	printk(KERN_INFO "PCA9554: ic2 cleanup module\n");
	xeno_i2c_exit();
	cdev_del(my_dev);
	printk(KERN_INFO "PCA9554: cleanup module terminated\n");
}

MODULE_LICENSE("GPL");
