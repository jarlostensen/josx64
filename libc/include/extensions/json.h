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
#ifdef _JOS_KERNEL_BUILD
#include <extensions/slices.h>
#else
#include <slices.h>
#endif
#include <collections.h>

typedef struct _json_context {

	size_t _keys_added;
	bool _obj_has_key;
	FILE* _stream;
	
} json_writer_context_t;

_JOS_API_FUNC void json_initialise_writer(json_writer_context_t* ctx, FILE* stream);
_JOS_API_FUNC void json_write_object_start(json_writer_context_t* writer_ctx);
_JOS_API_FUNC void json_write_object_end(json_writer_context_t* writer_ctx);
_JOS_API_FUNC void json_write_key(json_writer_context_t* writer_ctx, const char* key_name);
_JOS_API_FUNC void json_write_number(json_writer_context_t* writer_ctx, long long number);
_JOS_API_FUNC void json_write_string(json_writer_context_t* writer_ctx, const char* str);

typedef enum json_token_type {

	kJsonParse_Token_Null,
	kJsonParse_Token_Object,
	kJsonParse_Token_String,
	kJsonParse_Token_Number,
	kJsonParse_Token_KeyValueSeparator,

} json_token_type_t;

typedef struct _json_parse_token {
	json_token_type_t _type;
	char_array_slice_t	_slice;
} json_token_t;

_JOS_API_FUNC json_token_t* json_tokenise(vector_t* in_out_token_slices, char_array_slice_t in_json);
_JOS_API_FUNC json_token_t json_value(vector_t* tokens, const char* key);

#if defined(_JOS_IMPLEMENT_JSON) && !defined(_JOS_JSON_IMPLEMENTED)
#define _JOS_JSON_IMPLEMENTED

_JOS_API_FUNC  void json_initialise_writer(json_writer_context_t* ctx, FILE* stream) {
	memset(ctx, 0, sizeof(json_writer_context_t));
	ctx->_stream = stream;
	ctx->_keys_added = 0;
	ctx->_obj_has_key = false;
}

_JOS_API_FUNC void json_write_object_start(json_writer_context_t* writer_ctx) {
	fwrite("{ ", 1, 2, writer_ctx->_stream);
	writer_ctx->_obj_has_key = false;
}

_JOS_API_FUNC void json_write_object_end(json_writer_context_t* writer_ctx) {
	fwrite(" }", 1, 2, writer_ctx->_stream);
}

_JOS_API_FUNC void json_write_key(json_writer_context_t* writer_ctx, const char* key_name) {
	if (writer_ctx->_obj_has_key) {
		fwrite(", ", 1, 2, writer_ctx->_stream);
	}
	writer_ctx->_obj_has_key = true;
	++writer_ctx->_keys_added;
	fwrite("\"", 1, 1, writer_ctx->_stream);
	fwrite(key_name, 1, strlen(key_name), writer_ctx->_stream);
	fwrite("\"", 1, 1, writer_ctx->_stream);
	fwrite(" : ", 1, 3, writer_ctx->_stream);
}

_JOS_API_FUNC void json_write_number(json_writer_context_t* writer_ctx, long long number) {
	char buffer[32];
	int written = snprintf(buffer, sizeof(buffer), "%lld", number);
	fwrite(buffer, 1, written, writer_ctx->_stream);
}

_JOS_API_FUNC void json_write_string(json_writer_context_t* writer_ctx, const char* str) {
	fwrite("\"", 1, 1, writer_ctx->_stream);
	fwrite(str, 1, strlen(str), writer_ctx->_stream);
	fwrite("\"", 1, 1, writer_ctx->_stream);
}

// ====================================================================


typedef enum _json_parse_state {

	kJsonParse_Skip_Whitespace,
	kJsonParse_Identify_Next,
	kJsonParse_Parse_String,
	kJsonParse_Parse_Object,
	kJsonParse_Parse_Number,

} _json_parse_state_t;

_JOS_API_FUNC json_token_t* json_tokenise(vector_t* in_out_token_slices, char_array_slice_t in_json) {
	if (!in_json._length || !in_json._ptr)
		return 0;

	const char* rp = in_json._ptr;
	const char* end = in_json._ptr + in_json._length;

	_json_parse_state_t state = kJsonParse_Skip_Whitespace;
	_json_parse_state_t next_state = kJsonParse_Identify_Next;

	// we may be adding tokens to an existing list, so mark which one is "our" first one
	const size_t first_token = vector_size(in_out_token_slices);
	const char* start = rp;

	//TODO: error handling

	while (rp != end) {

		switch (state)
		{
		case kJsonParse_Skip_Whitespace:
		{
			while (rp!=end && (*rp == ' ' || *rp == '\t' || *rp == '\n')) ++rp;
			if (rp == end)
				break;
			state = next_state;
		}
		break;
		case kJsonParse_Identify_Next:
		{
			switch (*rp) {
			case ':':
			{
				// we only add this here to be able to validate the JSON, otherwise it's not really used
				json_token_t token;
				token._type = kJsonParse_Token_KeyValueSeparator;
				token._slice = kEmptySlice;
				vector_push_back(in_out_token_slices, &token);
				state = kJsonParse_Skip_Whitespace;
				next_state = kJsonParse_Identify_Next;
				// skip :
				++rp;
			}
			break;
			case '\"':
				state = kJsonParse_Parse_String;
				start = rp++;
				break;
			case '{':
				state = kJsonParse_Parse_Object;
				start = rp;
				break;
			case ',':
			{
				state = kJsonParse_Skip_Whitespace;
				next_state = kJsonParse_Identify_Next;
				// skip ,
				++rp;
			}
			break;
			default: 
			{
				if (*rp >= '0' && *rp <= '9') {
					state = kJsonParse_Parse_Number;
					start = rp; //< NO increment here
				}
				// TODO: ELSE ERROR
			}
			break;
			}			
		}
		break;
		case kJsonParse_Parse_Number:
		{
			while (rp!=end && *rp >= '0' && *rp <= '9') ++rp;
			if (rp == end)
				break;
			json_token_t token;
			char_array_slice_create(&token._slice, start, 0, rp - start);
			token._type = kJsonParse_Token_Number;
			vector_push_back(in_out_token_slices, &token);
			state = kJsonParse_Skip_Whitespace;
			next_state = kJsonParse_Identify_Next;
		}
		break;
		case kJsonParse_Parse_String:
		{
			while (rp != end && *rp != '\"') ++rp;
			if (rp == end)
				break;
			// we include the " in the token
			++rp;
			json_token_t token;
			char_array_slice_create(&token._slice, start, 0, rp - start);
			token._type = kJsonParse_Token_String;
			vector_push_back(in_out_token_slices, &token);
			state = kJsonParse_Skip_Whitespace;
			next_state = kJsonParse_Identify_Next;
		}
		break;
		case kJsonParse_Parse_Object:
		{
			int obj_level = 0;
			while (rp != end) {
				if (*rp == '{') {
					++obj_level;
				}
				if (*rp == '}') {
					if (--obj_level == 0) {
						// end of outermost object
						json_token_t token;
						//NOTE: the slice is *inside* the object so that we can parse it recursively
						char_array_slice_create(&token._slice, start, 1, rp - start - 1);
						//TODO: if slice is empty...
						token._type = kJsonParse_Token_Object;
						vector_push_back(in_out_token_slices, &token);
						state = kJsonParse_Skip_Whitespace;
						next_state = kJsonParse_Identify_Next;
						// skip }
						++rp;
						break;
					}
				}
				++rp;
			}
		}
		break;
		default:;
		}
	}

	return ((json_token_t*)vector_at(in_out_token_slices, first_token));
}

_JOS_API_FUNC json_token_t json_value(vector_t* tokens, const char* key) {

	int is_key = 1;
	bool found_key = false;
	json_token_t* obj_tokens = (json_token_t*)vector_data(tokens);
	json_token_t value = {._slice = kEmptySlice, ._type = kJsonParse_Token_Null };

	for (size_t i = 0; i < vector_size(tokens); ++i) {
		switch (obj_tokens[i]._type)
		{
		case kJsonParse_Token_Number:
		{
			if (found_key) {
				value._type = kJsonParse_Token_Number;
				value._slice = obj_tokens[i]._slice;
				return value;
			}
			is_key = 1;
		}
		break;
		case kJsonParse_Token_Object:
		{
			if (found_key) {
				value._type = kJsonParse_Token_Object;
				value._slice = obj_tokens[i]._slice;
				return value;
			}
			is_key = 1;
		}
		break;
		case kJsonParse_Token_String:
		{
			if (is_key) {
				if (char_array_slice_match_str(&obj_tokens[i]._slice, key)) {
					found_key = true;
				}
			}
			else {
				if (found_key) {
					value._type = kJsonParse_Token_String;
					value._slice = obj_tokens[i]._slice;
					return value;
				}
			}

			is_key ^= 1;
		}
		break;
		case kJsonParse_Token_KeyValueSeparator:
		{
			is_key = 0;
		}
		break;
		default:;
		}
	}

	return value;
}

#endif 
