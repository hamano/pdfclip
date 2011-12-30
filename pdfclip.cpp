/*
 * pdfclip - PDF cropping and trimming tool
 * Copyright (C) 2011 Tsukasa Hamano <code@cuspy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "PDFDoc.h"
#include "GlobalParams.h"
#include "splash/SplashBitmap.h"
#include "splash/Splash.h"
#include "SplashOutputDev.h"

PDFRectangle measure_margin(PDFDoc *doc, SplashOutputDev *dev, int page);

static int opt_debug = 0;
static int opt_page = 0;
static int opt_oreilly = 0;
static PDFRectangle *opt_margin = NULL;

void print_rect(PDFRectangle *rect)
{
	printf("%g, %g, %g, %g", rect->x1, rect->y1, rect->x2, rect->y2);
}

Object *mk_box_array(double x1, double y1, double x2, double y2)
{
	Object *obj_x1 = new Object();
	obj_x1->initReal(x1);
	Object *obj_y1 = new Object();
	obj_y1->initReal(y1);
	Object *obj_x2 = new Object();
	obj_x2->initReal(x2);
	Object *obj_y2 = new Object();
	obj_y2->initReal(y2);

	Object *array = new Object();
	array->initArray(NULL);
	array->arrayAdd(obj_x1);
	array->arrayAdd(obj_y1);
	array->arrayAdd(obj_x2);
	array->arrayAdd(obj_y2);
	obj_x1->free();
	return array;
}

void set_media_box(PDFDoc * doc, int page, PDFRectangle * rect)
{
	XRef *xref = doc->getXRef();
	//int num = xref->getNumObjects();
	Catalog *catalog = doc->getCatalog();
	Ref *ref = catalog->getPageRef(page);

	XRefEntry *entry = xref->getEntry(ref->num);
	Object obj;
	xref->fetch(ref->num, ref->gen, &obj);
	Object *newArray =
	    mk_box_array(rect->x1, rect->y1, rect->x2, rect->y2);
	obj.dictSet((char *) "MediaBox", newArray);
	entry->obj = obj;
	entry->updated = gTrue;
}

int pdfcrop_page(PDFDoc *doc, Catalog *catalog, SplashOutputDev *dev, int i) {
	PDFRectangle rect;
	Page *page = catalog->getPage(i);

	if(opt_margin){
		set_media_box(doc, i, opt_margin);
	}else{
		rect = measure_margin(doc, dev, i);
		set_media_box(doc, i, &rect);
	}

	if(opt_debug){
		printf("page=%d, box=[", i);
		print_rect(page->getMediaBox());
		printf("], crop=[");
		print_rect(&rect);
		printf("]\n");
	}
	return 0;
}

int pdfcrop(PDFDoc *doc) {
	SplashColor bgcolor;
	bgcolor[0] = 0;
	bgcolor[1] = 0;
	bgcolor[2] = 0;

	int page_num = doc->getNumPages();
	if(opt_debug){
		printf("Page Num: %d\n", page_num);
	}
	Catalog *catalog = doc->getCatalog();
	SplashOutputDev *dev;
	dev = new SplashOutputDev(splashModeMono8, 1, gTrue, bgcolor);
	dev->startDoc(doc->getXRef());

	if(opt_page){
		if(opt_page <= page_num){
			pdfcrop_page(doc, catalog, dev, opt_page);
		}
	}else{
		for (int i = 1; i <= page_num; i++) {
			pdfcrop_page(doc, catalog, dev, i);
		}
	}

	delete dev;
	return 0;
}

int main(int argc, char *argv[])
{
	int opt;
	PDFRectangle rect;

	while ((opt = getopt(argc, argv, "dop:m:")) != -1) {
		switch (opt) {
		case 'd':
			opt_debug++;
			break;
		case 'o':
			opt_oreilly = 1;
			break;
		case 'p':
			opt_page = atoi(optarg);
			break;
		case 'm':
			sscanf(optarg, "%lf %lf %lf %lf",
				   &rect.x1, &rect.y1, &rect.x2, &rect.y2);
			opt_margin = &rect;
			break;
		}
	}

	if (optind + 2 > argc) {
		fprintf(stderr, "Usage: %s [infile] [outfile]\n", argv[0]);
		fprintf(stderr, "\n");
		return EXIT_FAILURE;
	}

	globalParams = new GlobalParams();
	GooString *in_file = new GooString(argv[optind]);
	GooString *out_file = new GooString(argv[optind+1]);

	if(opt_debug){
		printf("Input File: %s\n", in_file->getCString());
		printf("Output File: %s\n", out_file->getCString());
	}

	//GooString *owner_pw = new GooString("");
	PDFDoc *doc = new PDFDoc(in_file, NULL, NULL);
	if (!doc->isOk()) {
		perror("pdf open error");
		return EXIT_FAILURE;
	}

	int ret;
	ret = pdfcrop(doc);
	if (ret) {
		printf("crop failed: %d\n", ret);
		return EXIT_FAILURE;
	}

	//ret = doc->saveAs(out_file, writeStandard);
	ret = doc->saveAs(out_file, writeForceRewrite);
	if (ret) {
		printf("save failed: %d\n", ret);
		return EXIT_FAILURE;
	}

	delete doc;
	// double free?
	//delete in_file;
	//delete out_file;
	// some times blocked.
	//delete globalParams;
	return EXIT_SUCCESS;
}

PDFRectangle measure_margin(PDFDoc * doc, SplashOutputDev * dev, int page)
{
	doc->displayPageSlice(dev, page, 72.0, 72.0,
						  gFalse, gFalse, gFalse, gFalse, 0, 0, -1, -1);
	SplashBitmap *bitmap = dev->getBitmap();

	//bitmap->writePNMFile((char*)"out.ppm");
/*
	printf("width: %d\n", bitmap->getWidth());
	printf("height: %d\n", bitmap->getHeight());
	printf("rowsize: %d\n", bitmap->getRowSize());
*/
	SplashColorPtr data = bitmap->getDataPtr();
	int width = bitmap->getWidth();
	int height = bitmap->getHeight();

	int margin_left = 0;
	int margin_top = 0;
	int margin_right = 0;
	int margin_bottom = 0;

	int x, y, i;
	if(opt_oreilly){
		// fill top
		for (y = 0; y < 70; y++) {
			for (x = 0; x < width; x++) {
				i = y * width + x;
				data[i] = 0;
			}
		}
		// fill bottom
		for (y = height - 1; y >= height - 20; y--) {
			for (x = 0; x < width; x++) {
				i = y * width + x;
				data[i] = 0;
			}
		}
		//bitmap->writePNMFile((char*)"out.ppm");
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			i = y * width + x;
			if (data[i]) {
				margin_top = y;
				goto out_top;
			}
		}
	}
out_top:

	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			i = y * width + x;
			if (data[i]) {
				margin_left = x;
				goto out_left;
			}
		}
	}
out_left:

	for (y = height - 1; y >= 0; y--) {
		for (x = 0; x < width; x++) {
			i = y * width + x;
			if (data[i]) {
				margin_bottom = height - y - 1;
				goto out_bottom;
			}
		}
	}
out_bottom:

	for (x = width - 1; x >= 0; x--) {
		for (y = 0; y < height; y++) {
			i = y * width + x;
			if (data[i]) {
				margin_right = width - x - 1;
				goto out_right;
			}
		}
	}
out_right:

/*
    printf("margin_top: %d\n", margin_top);
    printf("margin_left: %d\n", margin_left);
    printf("margin_bottom: %d\n", margin_bottom);
    printf("margin_right: %d\n", margin_right);
*/
	PDFRectangle rect;
	rect.x1 = margin_left;
	rect.y1 = margin_bottom;
	rect.x2 = width - margin_right;
	rect.y2 = height - margin_top;
	return rect;
}

/*
 * Local Variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim: sw=4 ts=4 sts=4 et
 */
