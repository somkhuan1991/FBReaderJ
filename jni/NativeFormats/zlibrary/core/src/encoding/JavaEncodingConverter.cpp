/*
 * Copyright (C) 2004-2012 Geometer Plus <contact@geometerplus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <AndroidUtil.h>
#include <JniEnvelope.h>
#include <ZLUnicodeUtil.h>

#include "JavaEncodingConverter.h"

class JavaEncodingConverter : public ZLEncodingConverter {

private:
	JavaEncodingConverter(const std::string &encoding);

public:
	~JavaEncodingConverter();
	std::string name() const;
	void convert(std::string &dst, const char *srcStart, const char *srcEnd);
	void reset();
	bool fillTable(int *map);

private:
	jobject myJavaConverter;
	int myBufferLength;
	jbyteArray myInBuffer;
	jbyteArray myOutBuffer;

friend class JavaEncodingConverterProvider;
};

bool JavaEncodingConverterProvider::providesConverter(const std::string &encoding) {
	if (encoding.empty()) {
		return false;
	}
	JNIEnv *env = AndroidUtil::getEnv();
	jobject collection = AndroidUtil::StaticMethod_JavaEncodingCollection_Instance->call(
		AndroidUtil::Class_JavaEncodingCollection->j()
	);
	jstring encodingName = AndroidUtil::createJavaString(env, encoding);
	jboolean result = AndroidUtil::Method_JavaEncodingCollection_providesConverterFor->call(collection, encodingName);
	env->DeleteLocalRef(encodingName);
	env->DeleteLocalRef(collection);
	return result != 0;
}

shared_ptr<ZLEncodingConverter> JavaEncodingConverterProvider::createConverter(const std::string &encoding) {
	return new JavaEncodingConverter(encoding);
}

JavaEncodingConverter::JavaEncodingConverter(const std::string &encoding) {
	JNIEnv *env = AndroidUtil::getEnv();
	jobject collection = AndroidUtil::StaticMethod_JavaEncodingCollection_Instance->call(
		AndroidUtil::Class_JavaEncodingCollection->j()
	);
	jstring encodingName = AndroidUtil::createJavaString(env, encoding);
	jobject javaEncoding = AndroidUtil::Method_JavaEncodingCollection_getEncoding_String->call(collection, encodingName);
	myJavaConverter = AndroidUtil::Method_Encoding_createConverter->call(javaEncoding);
	env->DeleteLocalRef(javaEncoding);
	env->DeleteLocalRef(encodingName);
	env->DeleteLocalRef(collection);

	myBufferLength = 32768;
	myInBuffer = env->NewByteArray(myBufferLength);
	myOutBuffer = env->NewByteArray(2 * myBufferLength);
}

JavaEncodingConverter::~JavaEncodingConverter() {
	JNIEnv *env = AndroidUtil::getEnv();
	env->DeleteLocalRef(myOutBuffer);
	env->DeleteLocalRef(myInBuffer);
	env->DeleteLocalRef(myJavaConverter);
}

std::string JavaEncodingConverter::name() const {
	JNIEnv *env = AndroidUtil::getEnv();
	jstring javaName = (jstring)env->GetObjectField(myJavaConverter, AndroidUtil::FID_EncodingConverter_Name);
	const std::string result = AndroidUtil::fromJavaString(env, javaName);
	env->DeleteLocalRef(javaName);
	return result;
}

void JavaEncodingConverter::convert(std::string &dst, const char *srcStart, const char *srcEnd) {
	JNIEnv *env = AndroidUtil::getEnv();
	const int srcLen = srcEnd - srcStart;
	if (srcLen > myBufferLength) {
		env->DeleteLocalRef(myOutBuffer);
		env->DeleteLocalRef(myInBuffer);
		myBufferLength = srcLen;
		myInBuffer = env->NewByteArray(myBufferLength);
		myOutBuffer = env->NewByteArray(2 * myBufferLength);
	}

	env->SetByteArrayRegion(myInBuffer, 0, srcLen, (jbyte*)srcStart);
	const jint dstLen = AndroidUtil::Method_EncodingConverter_convert->call(myJavaConverter, myInBuffer, 0, srcLen, myOutBuffer, 0);
	const int origLen = dst.size();
	dst.append(dstLen, '\0');
	env->GetByteArrayRegion(myOutBuffer, 0, dstLen, (jbyte*)dst.data() + origLen);
}

void JavaEncodingConverter::reset() {
	AndroidUtil::Method_EncodingConverter_reset->call(myJavaConverter);
}

bool JavaEncodingConverter::fillTable(int *map) {
	char in;
	std::string out;
	for (int i = 0; i < 256; ++i) {
		in = i;
		convert(out, &in, (&in)+1);
		reset();
		if (out.size() != 0) {
			ZLUnicodeUtil::Ucs4Char ch;
			ZLUnicodeUtil::firstChar(ch, out.data());
			map[i] = ch;
			out.clear();
		} else {
			map[i] = i;
		}
	}
	return true;
}
