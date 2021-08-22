#define _GNU_SOURCE
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <unistd.h>
#include <png.h>

#define QUEUE_LEN 1000
#define MSGSIZE (2 + 7 * 160)

#define DISPLAY_HOST "44.128.142.1"
#define DISPLAY_PORT 5005
#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080

struct msghdr msg[QUEUE_LEN];
struct iovec iovec[QUEUE_LEN];
uint8_t bufs[QUEUE_LEN][MSGSIZE];
int send_next = 0;
int next = 0;
int pos = 0;

static void
setup(void)
{
	for (int i=0;i<QUEUE_LEN;i++) {
		iovec[i].iov_base = bufs[i];
		msg[i].msg_iov = &iovec[i];
		msg[i].msg_iovlen = 1;
		bufs[i][0] = 0x00;
		bufs[i][1] = 0x01;
	}
}

static void
flush_frame(void)
{
	if (pos == 0) return;
	iovec[next].iov_len = 2 + (pos * 7);
	if (++next == QUEUE_LEN) next = 0;
}


static void
set_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t b, uint8_t g)
{
	uint8_t *buf = bufs[next] + 2 + pos * 7;
	buf[0] = x & 0xff;
	buf[1] = (x >> 8) & 0xff;
	buf[2] = y & 0xff;
	buf[3] = (y >> 8) & 0xff;
	buf[4] = r;
	buf[5] = b;
	buf[6] = g;
	if (++pos == 160) {
		flush_frame();
		pos = 0;
	}
}

static void
blank_screen(void)
{
	for (int x=0;x<DISPLAY_WIDTH;x++) {
		for (int y=0;y<DISPLAY_HEIGHT;y++) {
			set_pixel(x, y, 0, 0, 0);
		}
	}
}

struct data_png {
	int width, height;
	png_byte color_type;
	png_byte bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;
};

static struct data_png *
open_png(const char *pngname)
{
	char header[8];
	FILE *fp = fopen(pngname, "rb");
	struct data_png *d = malloc(sizeof(struct data_png));

	assert(fp);
	assert(d != NULL);

	fread(header, 1, 8, fp);
	assert(png_sig_cmp(header, 0, 8) == 0);
	d->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	assert(d->png_ptr);
	d->info_ptr = png_create_info_struct(d->png_ptr);
	assert(d->info_ptr);
	png_init_io(d->png_ptr, fp);
	png_set_sig_bytes(d->png_ptr, 8);
	png_read_info(d->png_ptr, d->info_ptr);
	d->width = png_get_image_width(d->png_ptr, d->info_ptr);
	d->height = png_get_image_height(d->png_ptr, d->info_ptr);
	d->color_type = png_get_color_type(d->png_ptr, d->info_ptr);
	d->bit_depth = png_get_bit_depth(d->png_ptr, d->info_ptr);

	d->number_of_passes = png_set_interlace_handling(d->png_ptr);
	png_read_update_info(d->png_ptr, d->info_ptr);

	d->row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * d->height);
	for (int y=0; y<d->height; y++)
		d->row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(d->png_ptr,d->info_ptr));

	png_read_image(d->png_ptr, d->row_pointers);

	fclose(fp);

	return d;
}

static void
draw_png(struct data_png *d, int x, int y)
{
	for (int sy=0; sy < d->height; sy++) {
		png_byte *row = d->row_pointers[sy];
		for (int sx=0; sx < d->width; sx++) {
			uint8_t *rbga = &row[sx*4];
			if (rbga[3] > 0)
				set_pixel(x+sx, y+sy, rbga[0], rbga[1], rbga[2]);
		}
	}
}

struct bb_info {
	int x;
	int y;
	int x1, y1, x2, y2;
	int move_x;
	int move_y;
	int rate;
	struct data_png *img;
	const char *imgfile;
};

static void
bb_init(struct bb_info *bb, struct data_png *img, int x, int y, int w, int h)
{
	bb->img = img;
	bb->x1 = x;
	bb->y1 = y;
	bb->x2 = w - img->width;
	bb->y2 = h - img->height;
	if (bb->x == -1) bb->x = (bb->x1 + bb->x2) / 2;
	if (bb->y == -1) bb->y = (bb->y1 + bb->y2) / 2;
}

static void
bb_draw(struct bb_info *bb)
{
	draw_png(bb->img, bb->x, bb->y);

	bb->x += bb->move_x;
	bb->y += bb->move_y;

	if (bb->x < bb->x1 || bb->x > bb->x2)
		bb->move_x *= -1;
	if (bb->y < bb->y1 || bb->y > bb->y2)
		bb->move_y *= -1;
}

struct bb_info images[] = {
// {
// 	.imgfile = "images/unicorn_cc.png",
// 	.move_x = 13,
// 	.move_y = -10,
// 	.x = -1,
// 	.y = -1,
// },
// {
// 	.imgfile = "images/windows_logo.png",
// 	.move_x = -8,
// 	.move_y = 3,
// 	.rate = 2,
// 	.x = -1,
// 	.y = -1,
// },
// {
// 	.imgfile = "images/spade.png",
// 	.move_x = 32,
// 	.move_y = -12,
// 	.x = 0,
// 	.y = 0,
// },
	{
		.imgfile = "images/plan.png",
		.move_x = 200,
		.move_y = 60,
		.rate = 2,
		.x = 1000,
		.y = 800,
	},
//{
//	.imgfile = "images/hackaday.png",
//	.move_x = 40,
//	.move_y = 18,
//	.rate = 3,
//	.x = 500,
//	.y = 800,
//},
	{}
};

static void
draw_frame(unsigned int frameno)
{
	for (struct bb_info *bb = &images[0]; bb->img; bb++) {
		if (bb->rate && frameno % bb->rate != 0)
			continue;
		bb_draw(bb);
	}
	flush_frame();
}

static void
send_stuff(int fd)
{
	int retval;
	int len;
	int from = send_next;
	if (next == send_next) {
		next = 0;
		send_next = 0;
		return;
	} else if (next > send_next) {
		len = next - send_next;
	} else {
		len = QUEUE_LEN - send_next;
	}

	for (int i=from; i<len; i++) {
		retval = sendmsg(fd, &msg[i], 0);
		if (retval == -1) {
			perror("sendmsg()");
			return;
		}
	}

	send_next += retval;
	if (send_next >= QUEUE_LEN) send_next = 0;
	//printf("%d/%d messages sent from %d, continuing from %d\n", retval, len, from, send_next);
}

int
main(void)
{
	int sockfd;
	struct sockaddr_in addr;
	struct hostent *hp;

	for (struct bb_info *bb = &images[0]; bb->imgfile; bb++) {
		bb_init(bb, open_png(bb->imgfile), 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
	}

	setup();

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}


	if ((hp = gethostbyname(DISPLAY_HOST)) == NULL) {
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(DISPLAY_PORT);
	addr.sin_addr.s_addr = *( u_long * ) hp->h_addr;

	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("connect()");
		exit(EXIT_FAILURE);
	}

	//blank_screen();
	unsigned int frame = 0;
	while (1) {
		//printf("draw_frame\n");
		draw_frame(frame++);
		int old = send_next;
		// for (int i=0;i<55;i++) {
		// 	send_stuff(sockfd);
		// 	send_next = old;
		// 	//usleep(15000);
		// }
		send_stuff(sockfd);
		usleep(10000);
		//break;
	}

	exit(0);
}
