/*
 * task_fb.c
 *
 *  Created on: 17 janv. 2012
 *      Author: redsuser
 */
#include "fb_task.h"
#include "vga_lookup.h"
#include "lcdlib.h"

#include "invaders_task.h"
#include "ship_task.h"
#include "hit_task.h"

#include "rt-app-m.h"
#include "vga_lookup.h"
#include <linux/slab.h>
/**
 * Variables privées
 */
RT_TASK fb_task_handle;
static uint8_t fb_task_created = 0;

// Variable locale pour les invaders
wave_t wave_loc;
// Variable locale pour les bullets
bullet_t bullets_loc[NB_MAX_BULLETS];
spaceship_t ship_loc;
uint32_t game_points_loc;

/**
 * Fonctions privées
 */
static void draw_bitmap(hitbox_t hb);
static void fb_task(void *cookie);
static void fb_task_init(void);
static void fb_task_update(void);

int fb_task_start() {
	int err;
	err = rt_task_create(&fb_task_handle, "task_fb", TASK_STKSZ, TASK_FB_PRIO,
			0);
	if (err == 0) {
		err = rt_task_start(&fb_task_handle, &fb_task, NULL);
		fb_task_created = 1;
		if (err != 0) {
			printk("rt-app: Task FB starting failed\n");
			goto fail;
		} else {
			printk("rt-app: Task FB starting succeed\n");
		}
	} else {
		printk("rt-app: Task FB creation failed\n");
		goto fail;
	}
	return 0;
	fail: fb_task_cleanup_task();
	fb_task_cleanup_objects();
	return -1;
}

void fb_task_cleanup_task() {
	if (fb_task_created) {
		printk("rt-app: Task FB cleanup task\n");
		fb_task_created = 0;
		rt_task_delete(&fb_task_handle);
	}
}

void fb_task_cleanup_objects() {
}

static void fb_task_init(){

}

static void fb_task_update(void){
	int i;
	// On copie les invaders en local
	invaders_lock();
	memcpy(&wave_loc, &wave, sizeof(wave_loc));
	invaders_unlock();

	// On copie le vaisseau en local
	ship_lock();
	memcpy(&ship_loc, &ship, sizeof(ship_loc));
	ship_unlock();

	// On copie les bullets en local
	hit_lock();
	memcpy(bullets_loc, bullets, sizeof(bullets_loc));
	game_points_loc = game_points;
	hit_unlock();

	// On dessine le background
	fb_rect_fill(10, 319, 0, 239, LU_BLACK);

	// On dessine les bullets
	for (i = 0; i < NB_MAX_BULLETS; i++) {
		if (bullets_loc[i].weapon != NULL) {
			draw_bitmap(bullets_loc[i].hitbox);
		}
	}

	// On dessine les bombs
	for (i = 0; i < NB_MAX_BOMBS; i++) {
		if (bombs[i].weapon != NULL) {
			draw_bitmap(bombs[i].hitbox);
		}
	}

	// On dessine les invaders
	for (i = 0; i < wave_loc.invaders_count; i++) {
		if (wave_loc.invaders[i].hp > 0) {
			draw_bitmap(wave_loc.invaders[i].hitbox);
		}
	}

	// On dessine le vaisseau
	draw_bitmap(ship_loc.hitbox);

	// On dessine le header
	fb_rect_fill(0, GAME_ZONE_Y_MIN, 0, GAME_ZONE_X_MAX - 1, LU_GREY);
	fb_line(0, GAME_ZONE_Y_MIN, GAME_ZONE_X_MAX, GAME_ZONE_Y_MIN,
			LU_WHITE);

	// On print le texte pour la progress bar
	fb_print_string(LU_BLACK, LU_GREY, "hp:", 3, 3);
	// On print la progress bar pour la vie
	fb_progress_bar(2, 10, 30, 150, LU_RED, ship_loc.hp, LIFE_SHIP);
	// On print le texte pour la progress bar
	fb_print_string(LU_BLACK, LU_GREY, "ac:", 3, 13);
	// On print la progress bar pour la precision
	fb_progress_bar(12, 20, 30, 150, LU_RED, game_bullet_kill,
			game_bullet_used);
}

static void fb_task(void *cookie) {


	uint8_t update = 1;

	uint8_t invader_bmp_select = 0;
	uint8_t invader_bmp_counter = 0;

	hitbox_t hitbox_invader_menu = { 52, // x
			70, // y
			136, // width
			99 // height
			};

	(void) cookie;

	// On définit la période de la tache
	rt_task_set_periodic(NULL, TM_NOW, 50 * MS);

	for (;;) {
		rt_task_wait_period(NULL);

		if(game_over){
			game_break = 1;

			fb_rect(30, 260, 30, 210, LU_WHITE);
			fb_rect_fill(31, 259, 31, 209, LU_GREY_BACK);

			// On affiche l'invader du menu
			if (invader_bmp_select == 0) {
				// Image 1
				hitbox_invader_menu.type = G_INVADER_MENU1;
				draw_bitmap(hitbox_invader_menu);

				if (invader_bmp_counter++ >= 10) {
					invader_bmp_counter = 0;
					// On change la valeur pour la prochaine image
					invader_bmp_select = 1;
				}
			} else {
				// Image 2
				hitbox_invader_menu.type = G_INVADER_MENU2;
				draw_bitmap(hitbox_invader_menu);

				if (invader_bmp_counter++ >= 10) {
					invader_bmp_counter = 0;
					// On change la valeur pour la prochaine image
					invader_bmp_select = 0;
				}
			}

			// On affiche la box de démarrage
			fb_print_string(LU_BLACK, LU_WHITE, "GAME OVER", 70, 50);

			// On affiche les crédits
			fb_print_string(LU_BLACK, LU_WHITE, "CREDITS:", 40, 70);
			fb_print_string(LU_DARK_GREY, LU_WHITE, "Michael Favaretto", 50, 85);
			fb_print_string(LU_DARK_GREY, LU_WHITE, "Yannick Lanz", 50, 95);
			fb_print_string(LU_DARK_GREY, LU_WHITE, "Romain Maffina", 50, 105);
			fb_print_string(LU_DARK_GREY, LU_WHITE, "Mohamed Regaya", 50, 115);

			// On affiche le bouton pour commencer
			fb_rect(200, 250, 40, 190, LU_GREY);
			fb_line(41, 201, 41, 249, LU_BLACK);
			fb_line(41, 249, 189, 249, LU_BLACK);
			fb_print_string(LU_GREY, LU_GREY, "RESTART", 80, 225);

			if(screen_pressed){
				screen_pressed = 0;

				// Bouton commencer
				if(screen_y >= 200 && screen_y <= 250 &&
				   screen_x >= 40  && screen_x <= 190){
					hit_task_init();
					ship_task_init();
					invaders_task_init();
					game_break = 0;
					game_over = 0;
				}
			}

		}else{
			if (!game_break) {
				fb_task_update();
				if(screen_pressed){
					screen_pressed = 0;

					if(screen_y <= 100){
						game_break = 1;
					}
				}
			} else {
				if(update){
					update = 0;
					fb_task_update();

					// On assombrit l'ecran
					int y, x;
					for (y = 0; y <= 319; y++) {
						for(x = 0; x <= 239; x++){
							*((unsigned short int*) (fb_mem_rt + 2 * x + y * 480)) &= (RED_SUBPIXEL(0x11) | GREEN_SUBPIXEL(0x11)| BLUE_SUBPIXEL(0x11));
						}
					}
				}

				if(game_started){
					// On dessine la box pour le menu (mode pause)
					//fb_rect(30, 190, 30, 210, LU_WHITE);
					//fb_rect_fill(31, 189, 31, 209, LU_GREY_BACK);
					fb_rect(30, 260, 30, 210, LU_WHITE);
					fb_rect_fill(31, 259, 31, 209, LU_GREY_BACK);
				}else{
					// On dessine la box pour le menu (mode demarrage)
					fb_rect(30, 260, 30, 210, LU_WHITE);
					fb_rect_fill(31, 259, 31, 209, LU_GREY_BACK);
				}

				// On affiche l'invader du menu
				if (invader_bmp_select == 0) {
					// Image 1
					hitbox_invader_menu.type = G_INVADER_MENU1;
					draw_bitmap(hitbox_invader_menu);

					if (invader_bmp_counter++ >= 10) {
						invader_bmp_counter = 0;
						// On change la valeur pour la prochaine image
						invader_bmp_select = 1;
					}
				} else {
					// Image 2
					hitbox_invader_menu.type = G_INVADER_MENU2;
					draw_bitmap(hitbox_invader_menu);

					if (invader_bmp_counter++ >= 10) {
						invader_bmp_counter = 0;
						// On change la valeur pour la prochaine image
						invader_bmp_select = 0;
					}
				}
				if(game_started){
					// On affiche la pause
					fb_print_string(LU_BLACK, LU_WHITE, "PAUSE", 100, 50);

					// On affiche les crédits
					fb_print_string(LU_BLACK, LU_WHITE, "CREDITS:", 40, 70);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Michael Favaretto", 50, 85);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Yannick Lanz", 50, 95);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Romain Maffina", 50, 105);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Mohamed Regaya", 50, 115);

					// On affiche le bouton pour reprendre le jeux
					fb_rect(200, 250, 40, 115, LU_GREY);
					fb_line(41, 201, 41, 249, LU_BLACK);
					fb_line(41, 249, 114, 249, LU_BLACK);
					fb_print_string(LU_GREY, LU_GREY, "CONTINUE", 45, 225);

					// On affiche le bouton pour recommencer le jeux
					fb_rect(200, 250, 120, 190, LU_GREY);
					fb_line(121, 201, 121, 249, LU_BLACK);
					fb_line(121, 249, 189, 249, LU_BLACK);
					fb_print_string(LU_GREY, LU_GREY, "RESTART", 125, 225);

					if(screen_pressed){
						screen_pressed = 0;

						if(screen_y >= 200 && screen_y <= 250){
							// Bouton continuer
							if(screen_x >= 40  && screen_x <= 115){
								game_started = 1;
								game_break = 0;
							// Bouton recommencer
							}else if(screen_x > 115  && screen_x <= 190){
								hit_task_init();
								ship_task_init();
								invaders_task_init();
								game_break = 0;
								printk("recommencer\n");
							}
						}
					}

					//fb_rect(70, 85, 40, 150, LU_GREY);
					//fb_rect_fill(31, 189, 31, 209, LU_WHITE);
				}else{
					// On affiche la box de démarrage
					fb_print_string(LU_BLACK, LU_WHITE, "SPACE INVADERS", 70, 50);

					// On affiche les crédits
					fb_print_string(LU_BLACK, LU_WHITE, "CREDITS:", 40, 70);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Michael Favaretto", 50, 85);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Yannick Lanz", 50, 95);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Romain Maffina", 50, 105);
					fb_print_string(LU_DARK_GREY, LU_WHITE, "Mohamed Regaya", 50, 115);

					// On affiche le bouton pour commencer
					fb_rect(200, 250, 40, 190, LU_GREY);
					fb_line(41, 201, 41, 249, LU_BLACK);
					fb_line(41, 249, 189, 249, LU_BLACK);
					fb_print_string(LU_GREY, LU_GREY, "START", 80, 225);

					if(screen_pressed){
						screen_pressed = 0;

						// Bouton commencer
						if(screen_y >= 200 && screen_y <= 250 &&
						   screen_x >= 40  && screen_x <= 190){
							game_started = 1;
							game_break = 0;
						}
					}
				}
			}
		}
		rt_task_set_priority(NULL, 90);
		fb_display();
		rt_task_set_priority(NULL, 70);
	}
}

static void draw_bitmap(hitbox_t hb) {
	int i, j;
	uint16_t *bmp[hb.height];

	switch (hb.type) {
	case G_SHIP:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_ship[i];
		}
		break;
	case G_INVADER:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_invader[i];
		}
		break;
	case G_BOMB:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_bomb[i];
		}
		break;
	case G_GUN:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_gun[i];
		}
		break;
	case G_RAIL:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_rail[i];
		}
		break;
	case G_ROCKET:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_rocket[i];
		}
		break;
	case G_WAVE:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_wave[i];
		}
		break;
	case G_INVADER_MENU1:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_invader_menu1[i];
		}
		break;
	case G_INVADER_MENU2:
		for (i = 0; i < hb.height; i++) {
			bmp[i] = bmp_invader_menu2[i];
		}
		break;
	}

	for (i = 0; i < hb.height; i++) {
		for (j = 0; j < hb.width; j++) {
			if ((*((*(bmp + i)) + j)) != LU_BLACK) {
				fb_set_pixel(hb.y + hb.height - i, hb.x + j,
						(*((*(bmp + i)) + j)));
			}
		}
	}
}

