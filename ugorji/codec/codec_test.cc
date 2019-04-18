#include <limits.h>
#include <gtest/gtest.h>
#include "simplecodec.h"
#include "binc.h"

/*
codecValueForTest is equivalent to:
struct x { int64_t A; bool B; uint64_t C; char* D; char* E; void* F; }
v = x{-9, true, 19, "-9true19", []byte{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, nil}
Expected when encoded (got from running $SRC/cmd/scratch/c_codec_test_in_go.go):
  simple:  len: 44, 0xf106d901410c09d9014203d901430813d90144d9082d39747275653139d90145e106aabbccddeeffd9014601
  msgpack: len: 33, 0x86a141f7a142c3a14313a144a82d39747275653139a145a6aabbccddeeffa146c0
  binc:    len: 35, 0x7a454120094542024543101345444c2d3974727565313945455aaabbccddeeff454600
*/

codec_value codecValueForTest(char* alphas) {
    codec_value cv;
    memset(&cv, 0, sizeof(codec_value));
    cv.type = CODEC_VALUE_MAP;
    cv.v.vMap.len = 6;
    cv.v.vMap.v = (codec_value_kv*)calloc(6, sizeof(codec_value_kv));
    // memset(&cv.v.vMap.v, '\0', 6 * sizeof(codec_value_kv));
    // char alphas[6*2] = {'A', '\0', 'B', '\0', 'C', '\0', 'D', '\0', 'E', '\0', 'F', '\0'};
    for(int i = 0; i < 6; i++) {
        alphas[i*2] = 'A' + i;
        // alphas[(i*2)+1] = '\0';
        cv.v.vMap.v[i].k.type = CODEC_VALUE_STRING;
        // cannot take pointer for loop variable, as it's same pointer for all. need to malloc.
        // auto s = std::string(1, (char)('A' + i));
        // cv.v.vMap.v[i].k.v.vString.vv = (char*)s.c_str();
        // cv.v.vMap.v[i].k.v.vString.v = (char*)malloc(2);
        // cv.v.vMap.v[i].k.v.vString.v[0] = (char)('A' + i);
        // cv.v.vMap.v[i].k.v.vString.v[1] = '\0';
        cv.v.vMap.v[i].k.v.vString.bytes.len = 1;
        cv.v.vMap.v[i].k.v.vString.bytes.v = &alphas[i*2];
        auto v2 = &cv.v.vMap.v[i].v;
        switch(i) {
        case 0:
            v2->type = CODEC_VALUE_NEG_INT;
            v2->v.vUint64 = 9;
            break;
        case 1:
            v2->type = CODEC_VALUE_BOOL;
            v2->v.vBool = true;
            break;
        case 2:
            v2->type = CODEC_VALUE_POS_INT;
            v2->v.vUint64 = 19;
            break;
        case 3:
        {
            v2->type = CODEC_VALUE_STRING;
            auto xv2 = "-9true19";
            v2->v.vString.bytes.v = (char*)&xv2[0];
            v2->v.vString.bytes.len = 8;
        }
            break;
        case 4:
        {
            char* xv2 = (char*)malloc(6);
            xv2[0] = 0xaa;
            xv2[1] = 0xbb;
            xv2[2] = 0xcc;
            xv2[3] = 0xdd;
            xv2[4] = 0xee;
            xv2[5] = 0xff;
            
            //dynamic allocation below was polluting other users
            //uint8_t xv2[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
            v2->type = CODEC_VALUE_BYTES;
            v2->v.vBytes.bytes.len = 6;
            v2->v.vBytes.bytes.v = &xv2[0];
        }
            break;
        case 5:
            v2->type = CODEC_VALUE_NIL;
            v2->v.vNil = true;
            break;
        }
    }
    return cv;
}

void encodeTest(codec_encode enc, codec_decode dec, size_t encLen) {
    const bool doPrint = false;
    const char* vv = "10:{ 7:A = 4:-9, 7:B = 2:1, 7:C = 3:19, 7:D = 7:-9true19, 7:E = 8:0xaabbccddeeff, 7:F = 1:1 }";
    char* alphas = (char*)malloc(12);
    codec_value cv = codecValueForTest(alphas);
    slice_bytes cb; // need to initialize it.
    memset(&cb, 0, sizeof(slice_bytes));
    char* err = nullptr;
    //ASSERT_TRUE(err == nullptr);
    enc(&cv, &cb, &err);
    // sometimes, ASSERT_STREQ does not show the error message
    if(doPrint) {
        if(err != nullptr) fprintf(stderr, "Err: %s\n", err);
        fprintf(stderr, "via encoded (encode): %ld, 0x", cb.bytes.len);
        for(size_t i = 0; i < cb.bytes.len; i++) {
            fprintf(stderr, "%.2hhx", cb.bytes.v[i]);
        }
        fprintf(stderr, "\n");
    }    
    ASSERT_EQ(encLen, cb.bytes.len);
    ASSERT_STREQ(nullptr, err);
    if(err != nullptr) free(err);
    err = nullptr;

    slice_bytes cb2;
    memset(&cb2, 0, sizeof(slice_bytes));
    codec_value_print(&cv, &cb2);
    if(doPrint) fprintf(stderr, "via print   (encode): %ld, %s\n", cb2.bytes.len, cb2.bytes.v);    

    ASSERT_STREQ(vv, cb2.bytes.v);
    ASSERT_EQ(strlen(vv), cb2.bytes.len);
    codec_value cv3;
    memset(&cv3, 0, sizeof (codec_value));
    dec(cb, &cv3, &err);
    if(err != nullptr) fprintf(stderr, "Err: %s\n", err);
    ASSERT_STREQ(nullptr, err);
    // ASSERT_TRUE(err == nullptr);
    // ASSERT_EQ(nullptr, err);
    if(err != nullptr) free(err);
    err = nullptr;
    
    slice_bytes cb3;
    memset(&cb3, 0, sizeof (slice_bytes));
    codec_value_print(&cv3, &cb3);
    if(doPrint) {
        fprintf(stderr, "via fprintf (decode): %ld, %s\n", cb3.bytes.len, cb3.bytes.v);
    }
    ASSERT_STREQ(vv, cb3.bytes.v);
    ASSERT_EQ(strlen(vv), cb3.bytes.len);
    free(alphas);
}

TEST(simplecodec, encode) {
    encodeTest(codec_simple_encode, codec_simple_decode, 44);
}

TEST(binc, encode) {
    // encodeTest(codec_binc_encode, codec_binc_decode, 35);
}

TEST(codec, codec_value_print) {
    // codec_value cv = {.type = CODEC_VALUE_MAP};
    // 9:{ 3:64 = 1:1 }
    codec_value cv;
    memset(&cv, 0, sizeof(codec_value));
    cv.type = CODEC_VALUE_MAP;
    cv.v.vMap.len = 1;
    cv.v.vMap.v = (codec_value_kv*)malloc(sizeof(codec_value_kv));
    cv.v.vMap.v[0].k.type = CODEC_VALUE_POS_INT;
    cv.v.vMap.v[0].k.v.vUint64 = 64;

    cv.v.vMap.v[0].v.type = CODEC_VALUE_NIL;
    cv.v.vMap.v[0].v.v.vNil = true;
    slice_bytes cb; // need to initialize it.
    memset(&cb, 0, sizeof(slice_bytes));
    codec_value_print(&cv, &cb);
    const char* vv = "10:{ 3:64 = 1:1 }";
    ASSERT_STREQ(cb.bytes.v, vv);
    // fprintf(stderr, "via fprintf: len: %lu, %s\n", cb.len, cb.v);
    // fprintf(stderr, "via write: ");
    // write(2, cb.v, cb.len);
    // fprintf(stderr, "\n");
    // EXPECT_EQ(1, 1);
    // ASSERT_TRUE(true);
}

