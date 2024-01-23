#include <v8.h>
#include "macros.h"
#include "common.h"

#include <cerrno>
#include <cstring>
#include <gd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define GD_TRUECOLOR 0
#define GD_PALETTE 1
#define GD_JPEG 2
#define GD_PNG 3
#define GD_GIF 4
#define GD_ANY 5
#define GD_WEBP 6
#define GD_PTR gdImagePtr ptr = LOAD_PTR(0, gdImagePtr)
#define GD_COLOR(offset) int color = args[offset]->Int32Value(JS_CONTEXT).ToChecked()
#define GD_RGB \
	int r = args[0]->Int32Value(JS_CONTEXT).ToChecked(); \
	int g = args[1]->Int32Value(JS_CONTEXT).ToChecked(); \
	int b = args[2]->Int32Value(JS_CONTEXT).ToChecked()
#define GD_RGBA \
	GD_RGB; \
	int a = args[3]->Int32Value(JS_CONTEXT).ToChecked()
#define GD_SECOND \
	v8::Local<v8::Object> __second = args[0]->ToObject(JS_CONTEXT).ToLocalChecked(); \
	gdImagePtr ptr2 = LOAD_PTR_FROM(__second, 0, gdImagePtr)

namespace {

/**
 * Creates a GD points structure from JS array of {x:..., y:...} objects
 */
gdPointPtr gdPoints(v8::Local<v8::Array> arr) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	unsigned int len = arr->Length();
	gdPointPtr points = new gdPoint[len];
	v8::Local<v8::Object> item;
	
	for (unsigned int i=0;i<len;i++) {
		item = arr->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToObject(JS_CONTEXT).ToLocalChecked();
		points[i].x = item->Get(JS_CONTEXT,JS_STR("x")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
		points[i].y = item->Get(JS_CONTEXT,JS_STR("y")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	}
	
	return points;
}

/**
 * Image constructor works in two modes:
 * a) new Image(Image.JPG|PNG|GIF, "filename.ext")
 * b) new Image(Image.TRUECOLOR|PALETTE, width, height)
 */
JS_METHOD(_image) {
	ASSERT_CONSTRUCTOR;

	int32_t type = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	gdImagePtr ptr;
	
	int x = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	
	void * data = NULL;
	size_t size = 0;
	
	/*if (type == GD_JPEG || type == GD_PNG || type == GD_GIF) {
		v8::String::Utf8Value name(JS_ISOLATE,args[1]);
		data = mmap_read(*name, &size);
		if (data == NULL) { JS_ERROR("Cannot open file"); return; }
	}*/

	switch (type) {
		case GD_TRUECOLOR:
			ptr = gdImageCreateTrueColor(x, y);
			break;
		case GD_ANY:
		case GD_JPEG:
		case GD_PNG:
		case GD_GIF:
		case GD_WEBP:
			{
				v8::String::Utf8Value name(JS_ISOLATE,args[1]);
				struct stat statbuf;
				int stat_ret=stat(*name,&statbuf);
				if (stat_ret!=0) {
					char tmp[1024];
					sprintf(tmp,"Cannot open file - %s",strerror(errno));
					JS_ERROR(tmp);
					return;
				}
				ptr=gdImageCreateFromFile(*name);
				if (!ptr) {
					JS_ERROR("Cannot load file - possibly corrupted or wrong extension");
					return;
				}
			}
			break;
		case GD_PALETTE:
			ptr = gdImageCreate(x, y);
			break;
		/*case GD_JPEG:
			ptr = gdImageCreateFromJpegPtr((int)size, data);
			break;
		case GD_PNG:
			ptr = gdImageCreateFromPngPtr((int)size, data);
			break;
		case GD_GIF:
			ptr = gdImageCreateFromGifPtr((int)size, data);
			break;*/
		default:
			JS_TYPE_ERROR("Unknown image type");
			return;
	}
	
	mmap_free((char *)data, size);
	SAVE_PTR(0, ptr);
	args.GetReturnValue().Set(args.This());
}

/**
 * @param {int} type Image.JPEG|PNG|GIF
 * @param {string} [file] File name. If not present, image data is returned as a Buffer
 */
JS_METHOD(_save) {
	GD_PTR;

	if (args.Length() < 1) {
		JS_ERROR("Invalid call format. Use 'image.save(type, [file])'");
		return;
	}
	
	int32_t type = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	bool tofile = args[1]->BooleanValue(JS_ISOLATE);
	int q = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	if (q == 0) { q = 95; }

	int size = 0;
	void * data = NULL;
	switch (type) {
		case GD_JPEG:
			data = gdImageJpegPtr(ptr, &size, q);
		break;
		
		case GD_GIF:
			data = gdImageGifPtr(ptr, &size);
		break;

		case GD_PNG:
			data = gdImagePngPtr(ptr, &size);
		break;

		case GD_WEBP:
			data = gdImageWebpPtr(ptr, &size);
		break;

		default:
			JS_TYPE_ERROR("Unknown image type");
			return;
	}

	if (tofile) {
		v8::String::Utf8Value name(JS_ISOLATE,args[1]);
		int result = mmap_write(*name, (void *)data, size);
		gdFree(data);
		if (result == -1) { JS_ERROR("Cannot open file"); return; }
		args.GetReturnValue().SetUndefined();
	} else {
		v8::Local<v8::Value> buffer = JS_BUFFER((char *)data, size);
		gdFree(data);
		args.GetReturnValue().Set(buffer);
	}
}

/**
 * All following functions are simple wrappers around gd* methods
 */

/**/

JS_METHOD(_truecolor) {
	GD_RGB;
	int result = gdTrueColor(r, g, b);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_truecoloralpha) {
	GD_RGBA;
	int result = gdTrueColorAlpha(r, g, b, a);
	args.GetReturnValue().Set(JS_INT(result));
}

/**/

JS_METHOD(_destroy) {
	GD_PTR;
	gdImageDestroy(ptr);
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_colorallocate) {
	GD_PTR;
	GD_RGB;
	
	int result = gdImageColorAllocate(ptr, r, g, b);
	if (result == -1) {
		JS_ERROR("Cannot allocate color");
	} else {
		args.GetReturnValue().Set(JS_INT(result));
	}
}

JS_METHOD(_colorallocatealpha) {
	GD_PTR;
	GD_RGBA;
	
	int result = gdImageColorAllocateAlpha(ptr, r, g, b, a);
	if (result == -1) {
		JS_ERROR("Cannot allocate color");
	} else {
		args.GetReturnValue().Set(JS_INT(result));
	}
}

JS_METHOD(_colorclosest) {
	GD_PTR;
	GD_RGB;
	
	int result = gdImageColorClosest(ptr, r, g, b);
	if (result == -1) {
		JS_ERROR("No collors allocated");
	} else {
		args.GetReturnValue().Set(JS_INT(result));
	}
}

JS_METHOD(_colorclosestalpha) {
	GD_PTR;
	GD_RGBA;
	
	int result = gdImageColorClosestAlpha(ptr, r, g, b, a);
	if (result == -1) {
		JS_ERROR("No collors allocated");
	} else {
		args.GetReturnValue().Set(JS_INT(result));
	}
}

JS_METHOD(_colorclosesthwb) {
	GD_PTR;
	GD_RGB;
	
	int result = gdImageColorClosestHWB(ptr, r, g, b);
	if (result == -1) {
		JS_ERROR("No collors allocated");
	} else {
		args.GetReturnValue().Set(JS_INT(result));
	}
}

JS_METHOD(_colorexact) {
	GD_PTR;
	GD_RGB;
	
	int result = gdImageColorExact(ptr, r, g, b);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_colorresolve) {
	GD_PTR;
	GD_RGB;
	
	int result = gdImageColorResolve(ptr, r, g, b);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_colorresolvealpha) {
	GD_PTR;
	GD_RGBA;
	
	int result = gdImageColorResolveAlpha(ptr, r, g, b, a);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_colorstotal) {
	GD_PTR;
	
	int result = gdImageColorsTotal(ptr);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_red) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = gdImageRed(ptr, c);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_green) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = gdImageGreen(ptr, c);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_blue) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = gdImageBlue(ptr, c);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_getinterlaced) {
	GD_PTR;
	
	int result = gdImageGetInterlaced(ptr);
	args.GetReturnValue().Set(JS_BOOL(result));
}

JS_METHOD(_gettransparent) {
	GD_PTR;
	
	int result = gdImageGetTransparent(ptr);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_colordeallocate) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageColorDeallocate(ptr, c);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_colortransparent) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageColorTransparent(ptr, c);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_alpha) {
	GD_PTR;
	int c = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = gdImageAlpha(ptr, c);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_getpixel) {
	GD_PTR;
	int x = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = gdImageGetPixel(ptr, x, y);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_boundssafe) {
	GD_PTR;
	int x = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[1]->Int32Value(JS_CONTEXT).ToChecked();

	int result = gdImageBoundsSafe(ptr, x, y);
	args.GetReturnValue().Set(JS_BOOL(result));
}

JS_METHOD(_sx) {
	GD_PTR;
	
	int result = gdImageSX(ptr);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_sy) {
	GD_PTR;
	
	int result = gdImageSY(ptr);
	args.GetReturnValue().Set(JS_INT(result));
}

JS_METHOD(_imagetruecolor) {
	GD_PTR;
	
	int result = gdImageTrueColor(ptr);
	args.GetReturnValue().Set(JS_BOOL(result));
}

/**/

JS_METHOD(_setpixel) {
	GD_PTR;
	GD_COLOR(2);
	int x = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageSetPixel(ptr, x, y, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_line) {
	GD_PTR;
	GD_COLOR(4);
	int x1 = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y1 = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int x2 = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int y2 = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageLine(ptr, x1, y1, x2, y2, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_polygon) {
	GD_PTR;
	GD_COLOR(1);

	if (!args[0]->IsArray()) { JS_TYPE_ERROR("Non-array argument passed to polygon()"); return; }
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	gdPointPtr points = gdPoints(arr);
	gdImagePolygon(ptr, points, arr->Length(), color);
	delete[] points;
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_openpolygon) {
	GD_PTR;
	GD_COLOR(1);
	
	if (!args[0]->IsArray()) { JS_TYPE_ERROR("Non-array argument passed to openPolygon()"); return; }
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	gdPointPtr points = gdPoints(arr);
	gdImageOpenPolygon(ptr, points, arr->Length(), color);
	delete[] points;
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_rectangle) {
	GD_PTR;
	GD_COLOR(4);
	
	int x1 = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y1 = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int x2 = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int y2 = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageRectangle(ptr, x1, y1, x2, y2, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_filledrectangle) {
	GD_PTR;
	GD_COLOR(4);
	
	int x1 = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y1 = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int x2 = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int y2 = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageFilledRectangle(ptr, x1, y1, x2, y2, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_filledpolygon) {
	GD_PTR;
	GD_COLOR(1);
	
	if (!args[0]->IsArray()) { JS_TYPE_ERROR("Non-array argument passed to filledPolygon()"); return; }
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	gdPointPtr points = gdPoints(arr);
	gdImageFilledPolygon(ptr, points, arr->Length(), color);
	delete[] points;
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_arc) {
	GD_PTR;
	GD_COLOR(6);
	int cx = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int cy = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int s = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int e = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageArc(ptr, cx, cy, w, h, s, e, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_filledarc) {
	GD_PTR;
	GD_COLOR(6);
	int cx = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int cy = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int s = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int e = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int style = args[7]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageFilledArc(ptr, cx, cy, w, h, s, e, color, style);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_filledellipse) {
	GD_PTR;
	GD_COLOR(4);
	int cx = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int cy = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageFilledEllipse(ptr, cx, cy, w, h, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_filltoborder) {
	GD_PTR;
	GD_COLOR(3);
	int x = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int border = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageFillToBorder(ptr, x, y, border, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_fill) {
	GD_PTR;
	GD_COLOR(2);
	int x = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageFill(ptr, x, y, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setantialiased) {
	GD_PTR;
	GD_COLOR(0);
	
	gdImageSetAntiAliased(ptr, color);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setantialiaseddontblend) {
	GD_PTR;
	GD_COLOR(0);
	int color2 = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	
	gdImageSetAntiAliasedDontBlend(ptr, color, color2);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setbrush) {
	GD_PTR;
	GD_SECOND;
	
	gdImageSetBrush(ptr, ptr2);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_settile) {
	GD_PTR;
	GD_SECOND;
	
	gdImageSetTile(ptr, ptr2);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setstyle) {
	GD_PTR;
	if (!args[0]->IsArray()) { JS_TYPE_ERROR("Non-array argument passed to setStyle()"); return; }
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	unsigned int len = arr->Length();
	
	int * style = new int[len];
	for (unsigned i=0;i<len;i++) {
		style[i] = arr->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	}
	 
	gdImageSetStyle(ptr, style, len);
	delete[] style;

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setthickness) {
	GD_PTR;
	int t = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	 
	gdImageSetThickness(ptr, t);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_alphablending) {
	GD_PTR;
	int mode = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	 
	gdImageAlphaBlending(ptr, mode);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_savealpha) {
	GD_PTR;
	int mode = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	 
	gdImageSaveAlpha(ptr, mode);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_setclip) {
	GD_PTR;
	int x1 = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int y1 = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int x2 = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int y2 = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	 
	gdImageSetClip(ptr, x1, y1, x2, y2);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_getclip) {
	GD_PTR;
	int x1 = 0;
	int y1 = 0;
	int x2 = 0;
	int y2 = 0;
	 
	gdImageGetClip(ptr, &x1, &y1, &x2, &y2);
	v8::Local<v8::Array> arr = v8::Array::New(JS_ISOLATE, 4);
	
	(void)arr->Set(JS_CONTEXT,JS_INT(0), JS_INT(x1));
	(void)arr->Set(JS_CONTEXT,JS_INT(1), JS_INT(y1));
	(void)arr->Set(JS_CONTEXT,JS_INT(2), JS_INT(x2));
	(void)arr->Set(JS_CONTEXT,JS_INT(3), JS_INT(y2));
	args.GetReturnValue().Set(arr);
}

JS_METHOD(_string) {
	GD_PTR;
	GD_COLOR(0);

	int brect[8];
	v8::String::Utf8Value font(JS_ISOLATE,args[1]);
	double size = args[2]->NumberValue(JS_CONTEXT).ToChecked();
	double angle = args[3]->NumberValue(JS_CONTEXT).ToChecked();
	int x = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int y = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	v8::String::Utf8Value str(JS_ISOLATE,args[6]);
	
	char * result = gdImageStringFT(ptr, &(brect[0]), color, *font, size, angle, x, y, *str);
	if (result == NULL) {
		v8::Local<v8::Array> arr = v8::Array::New(JS_ISOLATE, 8);
		for (int i=0;i<8;i++) {
			(void)arr->Set(JS_CONTEXT,JS_INT(i), JS_INT(brect[i]));
		}
		args.GetReturnValue().Set(arr);
	} else {
		JS_ERROR(result);
	}
}

JS_METHOD(_copy) {
	GD_PTR;
	GD_SECOND;
	
	int dstx = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int dsty = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[6]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopy(ptr, ptr2, dstx, dsty, srcx, srcy, w, h);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copyresized) {
	GD_PTR;
	GD_SECOND;
	
	int dstx = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int dsty = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int dstw = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int dsth = args[6]->Int32Value(JS_CONTEXT).ToChecked();
	int srcw = args[7]->Int32Value(JS_CONTEXT).ToChecked();
	int srch = args[8]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopyResized(ptr, ptr2, dstx, dsty, srcx, srcy, dstw, dsth, srcw, srch);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copyresampled) {
	GD_PTR;
	GD_SECOND;
	
	int dstx = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int dsty = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int dstw = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int dsth = args[6]->Int32Value(JS_CONTEXT).ToChecked();
	int srcw = args[7]->Int32Value(JS_CONTEXT).ToChecked();
	int srch = args[8]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopyResampled(ptr, ptr2, dstx, dsty, srcx, srcy, dstw, dsth, srcw, srch);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copyrotated) {
	GD_PTR;
	GD_SECOND;
	
	double dstx = args[1]->NumberValue(JS_CONTEXT).ToChecked();
	double dsty = args[2]->NumberValue(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int srcw = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int srch = args[6]->Int32Value(JS_CONTEXT).ToChecked();
	int angle = args[7]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopyRotated(ptr, ptr2, dstx, dsty, srcx, srcy, srcw, srch, angle);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copymerge) {
	GD_PTR;
	GD_SECOND;
	
	int dstx = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int dsty = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[6]->Int32Value(JS_CONTEXT).ToChecked();
	int pct = args[7]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopyMerge(ptr, ptr2, dstx, dsty, srcx, srcy, w, h, pct);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copymergegray) {
	GD_PTR;
	GD_SECOND;
	
	int dstx = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	int dsty = args[2]->Int32Value(JS_CONTEXT).ToChecked();
	int srcx = args[3]->Int32Value(JS_CONTEXT).ToChecked();
	int srcy = args[4]->Int32Value(JS_CONTEXT).ToChecked();
	int w = args[5]->Int32Value(JS_CONTEXT).ToChecked();
	int h = args[6]->Int32Value(JS_CONTEXT).ToChecked();
	int pct = args[7]->Int32Value(JS_CONTEXT).ToChecked();

	gdImageCopyMergeGray(ptr, ptr2, dstx, dsty, srcx, srcy, w, h, pct);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copypalette) {
	GD_PTR;
	GD_SECOND;
	
	gdImagePaletteCopy(ptr, ptr2);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_squaretocircle) {
	GD_PTR;
	
	int radius = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	gdImageSquareToCircle(ptr, radius);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_sharpen) {
	GD_PTR;
	
	int pct = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	gdImageSharpen(ptr, pct);
	args.GetReturnValue().Set(args.This());
}

}
/**/ 

SHARED_INIT() {
	//fprintf(stderr,"gd.cc > SHARED_INIT						isolate=%ld, InContext()=%d, context=%ld\n",(void*)JS_ISOLATE,JS_ISOLATE->InContext(),(void*)(*JS_CONTEXT));
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(JS_ISOLATE, _image);
	ft->SetClassName(JS_STR("Image"));
	
	/**
	 * Constants (Image.*)
	 */
	ft->Set(JS_ISOLATE,"TRUECOLOR"			, JS_INT(GD_TRUECOLOR));
	ft->Set(JS_ISOLATE,"PALETTE"			, JS_INT(GD_PALETTE));
	ft->Set(JS_ISOLATE,"MAXCOLORS"			, JS_INT(gdMaxColors));
	ft->Set(JS_ISOLATE,"JPEG"				, JS_INT(GD_JPEG));
	ft->Set(JS_ISOLATE,"PNG"				, JS_INT(GD_PNG));
	ft->Set(JS_ISOLATE,"GIF"				, JS_INT(GD_GIF));
	ft->Set(JS_ISOLATE,"ANY"				, JS_INT(GD_ANY));
	ft->Set(JS_ISOLATE,"WEBP"				, JS_INT(GD_WEBP));
	ft->Set(JS_ISOLATE,"ARC_ARC"			, JS_INT(gdArc));
	ft->Set(JS_ISOLATE,"ARC_PIE"			, JS_INT(gdPie));
	ft->Set(JS_ISOLATE,"ARC_CHORD"			, JS_INT(gdChord));
	ft->Set(JS_ISOLATE,"ARC_NOFILL"			, JS_INT(gdNoFill));
	ft->Set(JS_ISOLATE,"ARC_EDGED"			, JS_INT(gdEdged));
	ft->Set(JS_ISOLATE,"COLOR_ANTIALIASED"	, JS_INT(gdAntiAliased));
	ft->Set(JS_ISOLATE,"COLOR_BRUSHED"		, JS_INT(gdBrushed));
	ft->Set(JS_ISOLATE,"COLOR_STYLED"		, JS_INT(gdStyled));
	ft->Set(JS_ISOLATE,"COLOR_STYLEDBRUSHED", JS_INT(gdStyledBrushed));
	ft->Set(JS_ISOLATE,"COLOR_TILED"		, JS_INT(gdTiled));
	ft->Set(JS_ISOLATE,"COLOR_TRANSPARENT"	, JS_INT(gdTransparent));
	
	/**
	 * Static methods (Image.*)
	 */
	ft->Set(JS_ISOLATE,"trueColor"			, v8::FunctionTemplate::New(JS_ISOLATE, _truecolor));
	ft->Set(JS_ISOLATE,"trueColorAlpha"		, v8::FunctionTemplate::New(JS_ISOLATE, _truecoloralpha));

	v8::Local<v8::ObjectTemplate> it = ft->InstanceTemplate();
	it->SetInternalFieldCount(1);

	v8::Local<v8::ObjectTemplate> pt = ft->PrototypeTemplate();
	
	/**
	 * Prototype methods (new Image().*)
	 */
	pt->Set(JS_ISOLATE,"save"					, v8::FunctionTemplate::New(JS_ISOLATE, _save));
	
	pt->Set(JS_ISOLATE,"colorAllocate"			, v8::FunctionTemplate::New(JS_ISOLATE, _colorallocate));
	pt->Set(JS_ISOLATE,"colorAllocateAlpha"		, v8::FunctionTemplate::New(JS_ISOLATE, _colorallocatealpha));
	pt->Set(JS_ISOLATE,"colorClosest"			, v8::FunctionTemplate::New(JS_ISOLATE, _colorclosest));
	pt->Set(JS_ISOLATE,"colorClosestAlpha"		, v8::FunctionTemplate::New(JS_ISOLATE, _colorclosestalpha));
	pt->Set(JS_ISOLATE,"colorClosestHWB"		, v8::FunctionTemplate::New(JS_ISOLATE, _colorclosesthwb));
	pt->Set(JS_ISOLATE,"colorExact"				, v8::FunctionTemplate::New(JS_ISOLATE, _colorexact));
	pt->Set(JS_ISOLATE,"colorResolve"			, v8::FunctionTemplate::New(JS_ISOLATE, _colorresolve));
	pt->Set(JS_ISOLATE,"colorResolveAlpha"		, v8::FunctionTemplate::New(JS_ISOLATE, _colorresolvealpha));
	pt->Set(JS_ISOLATE,"colorsTotal"			, v8::FunctionTemplate::New(JS_ISOLATE, _colorstotal));
	pt->Set(JS_ISOLATE,"red"					, v8::FunctionTemplate::New(JS_ISOLATE, _red));
	pt->Set(JS_ISOLATE,"green"					, v8::FunctionTemplate::New(JS_ISOLATE, _green));
	pt->Set(JS_ISOLATE,"blue"					, v8::FunctionTemplate::New(JS_ISOLATE, _blue));
	pt->Set(JS_ISOLATE,"getInterlaced"			, v8::FunctionTemplate::New(JS_ISOLATE, _getinterlaced));
	pt->Set(JS_ISOLATE,"getTransparent"			, v8::FunctionTemplate::New(JS_ISOLATE, _gettransparent));
	pt->Set(JS_ISOLATE,"getPixel"				, v8::FunctionTemplate::New(JS_ISOLATE, _getpixel));
	pt->Set(JS_ISOLATE,"colorDeallocate"		, v8::FunctionTemplate::New(JS_ISOLATE, _colordeallocate));
	pt->Set(JS_ISOLATE,"colorTransparent"		, v8::FunctionTemplate::New(JS_ISOLATE, _colortransparent));
	pt->Set(JS_ISOLATE,"alpha"					, v8::FunctionTemplate::New(JS_ISOLATE, _alpha));
	pt->Set(JS_ISOLATE,"boundsSafe"				, v8::FunctionTemplate::New(JS_ISOLATE, _boundssafe));
	pt->Set(JS_ISOLATE,"sx"						, v8::FunctionTemplate::New(JS_ISOLATE, _sx));
	pt->Set(JS_ISOLATE,"sy"						, v8::FunctionTemplate::New(JS_ISOLATE, _sy));
	pt->Set(JS_ISOLATE,"trueColor"				, v8::FunctionTemplate::New(JS_ISOLATE, _imagetruecolor));

	pt->Set(JS_ISOLATE,"setPixel"				, v8::FunctionTemplate::New(JS_ISOLATE, _setpixel));
	pt->Set(JS_ISOLATE,"line"					, v8::FunctionTemplate::New(JS_ISOLATE, _line));
	pt->Set(JS_ISOLATE,"polygon"				, v8::FunctionTemplate::New(JS_ISOLATE, _polygon));
	pt->Set(JS_ISOLATE,"openPolygon"			, v8::FunctionTemplate::New(JS_ISOLATE, _openpolygon));
	pt->Set(JS_ISOLATE,"rectangle"				, v8::FunctionTemplate::New(JS_ISOLATE, _rectangle));
	pt->Set(JS_ISOLATE,"filledPolygon"			, v8::FunctionTemplate::New(JS_ISOLATE, _filledpolygon));
	pt->Set(JS_ISOLATE,"filledRectangle"		, v8::FunctionTemplate::New(JS_ISOLATE, _filledrectangle));
	pt->Set(JS_ISOLATE,"arc"					, v8::FunctionTemplate::New(JS_ISOLATE, _arc));
	pt->Set(JS_ISOLATE,"filledArc"				, v8::FunctionTemplate::New(JS_ISOLATE, _filledarc));
	pt->Set(JS_ISOLATE,"filledEllipse"			, v8::FunctionTemplate::New(JS_ISOLATE, _filledellipse));
	pt->Set(JS_ISOLATE,"fillToBorder"			, v8::FunctionTemplate::New(JS_ISOLATE, _filltoborder));
	pt->Set(JS_ISOLATE,"fill"					, v8::FunctionTemplate::New(JS_ISOLATE, _fill));
	pt->Set(JS_ISOLATE,"setAntialiased"			, v8::FunctionTemplate::New(JS_ISOLATE, _setantialiased));
	pt->Set(JS_ISOLATE,"setAntialiasedDontBlend", v8::FunctionTemplate::New(JS_ISOLATE, _setantialiaseddontblend));
	pt->Set(JS_ISOLATE,"setBrush"				, v8::FunctionTemplate::New(JS_ISOLATE, _setbrush));
	pt->Set(JS_ISOLATE,"setTile"				, v8::FunctionTemplate::New(JS_ISOLATE, _settile));
	pt->Set(JS_ISOLATE,"setStyle"				, v8::FunctionTemplate::New(JS_ISOLATE, _setstyle));
	pt->Set(JS_ISOLATE,"setThickness"			, v8::FunctionTemplate::New(JS_ISOLATE, _setthickness));
	pt->Set(JS_ISOLATE,"alphaBlending"			, v8::FunctionTemplate::New(JS_ISOLATE, _alphablending));
	pt->Set(JS_ISOLATE,"saveAlpha"				, v8::FunctionTemplate::New(JS_ISOLATE, _savealpha));
	pt->Set(JS_ISOLATE,"setClip"				, v8::FunctionTemplate::New(JS_ISOLATE, _setclip));
	pt->Set(JS_ISOLATE,"getClip"				, v8::FunctionTemplate::New(JS_ISOLATE, _getclip));
	pt->Set(JS_ISOLATE,"string"					, v8::FunctionTemplate::New(JS_ISOLATE, _string));

	pt->Set(JS_ISOLATE,"copy"					, v8::FunctionTemplate::New(JS_ISOLATE, _copy));
	pt->Set(JS_ISOLATE,"copyResized"			, v8::FunctionTemplate::New(JS_ISOLATE, _copyresized));
	pt->Set(JS_ISOLATE,"copyResampled"			, v8::FunctionTemplate::New(JS_ISOLATE, _copyresampled));
	pt->Set(JS_ISOLATE,"copyRotated"			, v8::FunctionTemplate::New(JS_ISOLATE, _copyrotated));
	pt->Set(JS_ISOLATE,"copyMerge"				, v8::FunctionTemplate::New(JS_ISOLATE, _copymerge));
	pt->Set(JS_ISOLATE,"copyMergeGray"			, v8::FunctionTemplate::New(JS_ISOLATE, _copymergegray));
	pt->Set(JS_ISOLATE,"copyPalette"			, v8::FunctionTemplate::New(JS_ISOLATE, _copypalette));
	pt->Set(JS_ISOLATE,"squareToCircle"			, v8::FunctionTemplate::New(JS_ISOLATE, _squaretocircle));
	pt->Set(JS_ISOLATE,"sharpen"				, v8::FunctionTemplate::New(JS_ISOLATE, _sharpen));

	(void)exports->Set(JS_CONTEXT,JS_STR("Image"), ft->GetFunction(JS_CONTEXT).ToLocalChecked());
}
