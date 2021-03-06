#include "rgba.h"
#include "RW/RenderWare.h"

CRGBA::CRGBA(unsigned char red, unsigned char green, unsigned char blue) {
    Set(red, green, blue, 0xFF);
}

CRGBA::CRGBA(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    Set(red, green, blue, alpha);
}

CRGBA::CRGBA(CRGBA const &rhs) {
    Set(rhs);
}

CRGBA::CRGBA(unsigned int intValue) {
    Set(intValue);
}

CRGBA::CRGBA() 
{
	r = 0;
	b = 0;
	b = 0;
	a = 0;
}

void CRGBA::Set(unsigned char red, unsigned char green, unsigned char blue) {
    r = red;
    g = green;
    b = blue;
}

void CRGBA::Set(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    Set(red, green, blue);
    a = alpha;
}

void CRGBA::Set(unsigned int intValue) {
    r = (intValue >> 24) & 0xFF;
    g = (intValue >> 16) & 0xFF;
    b = (intValue >> 8) & 0xFF;
    a = intValue & 0xFF;
}

void CRGBA::Set(CRGBA const &rhs) {
    Set(rhs.r, rhs.g, rhs.b, rhs.a);
}

void CRGBA::Set(CRGBA const &rhs, unsigned char alpha) {
    Set(rhs.r, rhs.g, rhs.b, alpha);
}

unsigned int CRGBA::ToInt() const {
    return a | (b << 8) | (g << 16) | (r << 24);
}

unsigned int CRGBA::ToIntARGB() const {
    return b | (g << 8) | (r << 16) | (a << 24);
}

void CRGBA::FromARGB(unsigned int intValue) {
    a = (intValue >> 24) & 0xFF;
    r = (intValue >> 16) & 0xFF;
    g = (intValue >> 8) & 0xFF;
    b = intValue & 0xFF;
}

void CRGBA::FromABGR(unsigned int intValue) {
	a = (intValue >> 24) & 0xFF;
	b = (intValue >> 16) & 0xFF;
	g = (intValue >> 8) & 0xFF;
	r = intValue & 0xFF;	
}

void CRGBA::Invert() {
    Set(0xFF - r, 0xFF - g, 0xFF - b);
}

CRGBA CRGBA::Inverted() const {
    CRGBA invertedColor = *this;
    invertedColor.Invert();
    return invertedColor;
}

bool CRGBA::operator==(CRGBA const &rhs) const {
    return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
}

CRGBA &CRGBA::operator=(CRGBA const &rhs) {
    Set(rhs);
    return *this;
}

CRGBA CRGBA::ToRGB() const {
    return CRGBA(r, g, b, 0xFF);
}