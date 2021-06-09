#pragma once

// super lightweight JSON stream writer
// example usage:
// 
// json_writer_context_t ctx;
// json_initialise_writer(&ctx, stdout);
//    json_write_object_start(&ctx);
//        json_write_key(&ctx, "version");
//		json_write_object_start(&ctx);
//		    json_write_key(&ctx, "major");
//		    json_write_number(&ctx, 0);
//			json_write_key(&ctx, "minor");
//			json_write_number(&ctx, 1);
//			json_write_key(&ctx, "patch");
//			json_write_number(&ctx, 0);
//		json_write_object_end(&ctx);
//        json_write_key(&ctx, "image_info");
//        json_write_object_start(&ctx);
//            json_write_key(&ctx, "base");
//            json_write_number(&ctx, 0x12345678abcdef00);
//        json_write_object_end(&ctx);
//        json_write_key(&ctx, "foo");
//        json_write_string(&ctx, "bar");
//    json_write_object_end(&ctx);
//
// the code above produces: 
//    { version : { major : 0, minor : 1, patch : 0 }, image_info : { base : 1311768467750121216 }, foo : "bar"}
//
#include <stdio.h>
#include <string.h>
#include <jos.h>

typedef struct _json_context {

	size_t _keys_added;
	bool _obj_has_key;
	FILE* _stream;

} json_writer_context_t;

_JOS_INLINE_FUNC void json_initialise_writer(json_writer_context_t* ctx, FILE* stream) {
	memset(ctx, 0, sizeof(json_writer_context_t));
	ctx->_stream = stream;
	ctx->_keys_added = 0;
	ctx->_obj_has_key = false;
}

_JOS_INLINE_FUNC void json_write_object_start(json_writer_context_t* writer_ctx) {
	fwrite("{ ", 1, 2, writer_ctx->_stream);
	writer_ctx->_obj_has_key = false;
}

_JOS_INLINE_FUNC void json_write_object_end(json_writer_context_t* writer_ctx) {
	fwrite(" }", 1, 2, writer_ctx->_stream);
}

_JOS_INLINE_FUNC void json_write_key(json_writer_context_t* writer_ctx, const char* key_name) {
	if (writer_ctx->_obj_has_key) {
		fwrite(", ", 1, 2, writer_ctx->_stream);
	}
	writer_ctx->_obj_has_key = true;
	++writer_ctx->_keys_added;
	fwrite(key_name, 1, strlen(key_name), writer_ctx->_stream);
	fwrite(" : ", 1, 3, writer_ctx->_stream);
}

_JOS_INLINE_FUNC void json_write_number(json_writer_context_t* writer_ctx, long long number) {
	char buffer[32];
	int written = sprintf(buffer, "%lld", number);
	fwrite(buffer, 1, written, writer_ctx->_stream);
}

_JOS_INLINE_FUNC void json_write_string(json_writer_context_t* writer_ctx, const char* str) {
	fwrite("\"", 1, 1, writer_ctx->_stream);
	fwrite(str, 1, strlen(str), writer_ctx->_stream);
	fwrite("\"", 1, 1, writer_ctx->_stream);
}
