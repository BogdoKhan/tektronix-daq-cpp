#pragma once

#include <string>

char* str_to_ch (const std::string& buf) {
	return const_cast<char*>(buf.c_str());
}

const char* ch_to_conch (char* buf) {
	return const_cast<const char*>(buf);
}
const char* conch_to_ch (const char* buf) {
	return const_cast<char*>(buf);
}

unsigned char* ch_to_uch (char* buf) {
	return reinterpret_cast<unsigned char*>(buf);
}
char* uch_to_ch (unsigned char* buf) {
	return reinterpret_cast<char*>(buf);
}

const unsigned char* conch_to_conuch(const char* buf) {
    return reinterpret_cast<const unsigned char*>(buf);
}

const char* conuch_to_conch(const unsigned char* buf) {
    return reinterpret_cast<const char*>(buf);
}

const unsigned char* uch_to_conuch (unsigned char* buf) {
	return const_cast<const unsigned char*>(buf);
}

unsigned char* conuch_to_uch (const unsigned char* buf) {
	return const_cast<unsigned char*>(buf);
}

//cast string
//char* str_to_ch (const std::string& buf)

const char* str_to_conch (const std::string& buf) {
    return buf.c_str();
}

const unsigned char* str_to_conuch (const std::string& buf) {
    return conch_to_conuch(str_to_conch(buf));
}

unsigned char* str_to_uch(const std::string& buf) {
    return ch_to_uch(str_to_ch(buf));
}
