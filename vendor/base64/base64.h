#include <string>

// modified base64 by ZTzTopia's
static std::string base64_encode(const std::string &text) 
{
	std::string out;

    const std::string chars =
        "ADQCSNPTRVZFUYHEXJWOMLGBIK"
        "baezdvguqfypnjwtohrmxkscl"
        "9132870456-_";

    unsigned char const* bytes_to_encode = (unsigned char const *)text.c_str();
    unsigned int in_len = (unsigned int)text.size();
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    out.clear();

    while( in_len-- ) {
    	char_array_3[i++] = *(bytes_to_encode++);
        if( i == 3 ) {
            char_array_4[0] =  (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] =   char_array_3[2] & 0x3f;

            for( i = 0; (i <4) ; i++ )
                out += chars[char_array_4[i]];
            i = 0;
        }
    }

    if( i ) {
        for( j = i; j < 3; j++ )
            char_array_3[j] = '\0';

        char_array_4[0] =  (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] =   char_array_3[2] & 0x3f;

        for( j = 0; (j < i + 1); j++ )
            out += chars[char_array_4[j]];

        /*while( (i++ < 3) )
            out += '=';*/
    }

    return out;
}

static std::string base64_decode(const std::string &encoded) 
{
	std::string out;

    const std::string chars =
        "ADQCSNPTRVZFUYHEXJWOMLGBIK"
        "baezdvguqfypnjwtohrmxkscl"
        "9132870456-_";

    unsigned int in_len = (unsigned int)encoded.size();
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    out.clear();

    while (in_len--/* && ( encoded[in_] != '=')*/ && //is_base64(encoded[in_])) {
        (isalnum(encoded[in_]) || encoded[in_] == '-' || encoded[in_] == '_')) {
        char_array_4[i++] = encoded[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = chars.find(char_array_4[i]);

            char_array_3[0] =  (char_array_4[0] << 2)        + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                out += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = chars.find(char_array_4[j]);

        char_array_3[0] =  (char_array_4[0] << 2)        + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) out += char_array_3[j];
    }

    return out;
}