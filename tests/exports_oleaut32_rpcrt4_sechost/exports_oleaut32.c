#include "exp.h"

static const exp_ent tbl[] = {
  {"SysAllocString", 2, 74464u, NULL},
  {"SysReAllocString", 3, 7632u, NULL},
  {"SysAllocStringLen", 4, 74240u, NULL},
  {"SysReAllocStringLen", 5, 81104u, NULL},
  {"SysFreeString", 6, 74864u, NULL},
  {"SysStringLen", 7, 16560u, NULL},
  {"VariantInit", 8, 609504u, NULL},
  {"VariantClear", 9, 610240u, NULL},
  {"VariantCopy", 10, 609712u, NULL},
  {"VariantCopyInd", 11, 609536u, NULL},
  {"VariantChangeType", 12, 70528u, NULL},
  {"VariantTimeToDosDateTime", 13, 390496u, NULL},
  {"DosDateTimeToVariantTime", 14, 120768u, NULL},
  {"SafeArrayCreate", 15, 15968u, NULL},
  {"SafeArrayDestroy", 16, 69424u, NULL},
  {"SafeArrayGetDim", 17, 21392u, NULL},
  {"SafeArrayGetElemsize", 18, 23296u, NULL},
  {"SafeArrayGetUBound", 19, 16592u, NULL},
  {"SafeArrayGetLBound", 20, 20880u, NULL},
  {"SafeArrayLock", 21, 30032u, NULL},
  {"SafeArrayUnlock", 22, 19392u, NULL},
  {"SafeArrayAccessData", 23, 19440u, NULL},
  {"SafeArrayUnaccessData", 24, 21136u, NULL},
  {"SafeArrayGetElement", 25, 68464u, NULL},
  {"SafeArrayPutElement", 26, 68944u, NULL},
  {"SafeArrayCopy", 27, 86640u, NULL},
  {"DispGetParam", 28, 387984u, NULL},
  {"DispGetIDsOfNames", 29, 12880u, NULL},
  {"DispInvoke", 30, 14032u, NULL},
  {"CreateDispTypeInfo", 31, 387056u, NULL},
  {"CreateStdDispatch", 32, 387968u, NULL},
  {"RegisterActiveObject", 33, 390752u, NULL},
  {"RevokeActiveObject", 34, 390864u, NULL},
  {"GetActiveObject", 35, 390656u, NULL},
  {"SafeArrayAllocDescriptor", 36, 87680u, NULL},
  {"SafeArrayAllocData", 37, 93872u, NULL},
  {"SafeArrayDestroyDescriptor", 38, 29888u, NULL},
  {"SafeArrayDestroyData", 39, 417712u, NULL},
  {"SafeArrayRedim", 40, 88864u, NULL},
  {"SafeArrayAllocDescriptorEx", 41, 28416u, NULL},
  {"SafeArrayCreateEx", 42, 416736u, NULL},
  {"SafeArrayCreateVectorEx", 43, 417056u, NULL},
  {"SafeArraySetRecordInfo", 44, 417600u, NULL},
  {"SafeArrayGetRecordInfo", 45, 417488u, NULL},
  {"VarParseNumFromStr", 46, 105328u, NULL},
  {"VarNumFromParseNum", 47, 107328u, NULL},
  {"VarI2FromUI1", 48, 379680u, NULL},
  {"VarI2FromI4", 49, 379360u, NULL},
  {"VarI2FromR4", 50, 379456u, NULL},
  {"VarI2FromR8", 51, 379472u, NULL},
  {"VarI2FromCy", 52, 435072u, NULL},
  {"VarI2FromDate", 53, 379248u, NULL},
  {"VarI2FromStr", 54, 379536u, NULL},
  {"VarI2FromDisp", 55, 379264u, NULL},
  {"VarI2FromBool", 56, 379232u, NULL},
  {"SafeArraySetIID", 57, 118816u, NULL},
  {"VarI4FromUI1", 58, 379984u, NULL},
  {"VarI4FromI2", 59, 379792u, NULL},
  {"VarI4FromR4", 60, 379968u, NULL},
  {"VarI4FromR8", 61, 5632u, NULL},
  {"VarI4FromCy", 62, 435152u, NULL},
  {"VarI4FromDate", 63, 379808u, NULL},
  {"VarI4FromStr", 64, 105104u, NULL},
  {"VarI4FromDisp", 65, 379824u, NULL},
  {"VarI4FromBool", 66, 379792u, NULL},
  {"SafeArrayGetIID", 67, 417440u, NULL},
  {"VarR4FromUI1", 68, 380992u, NULL},
  {"VarR4FromI2", 69, 380752u, NULL},
  {"VarR4FromI4", 70, 380928u, NULL},
  {"VarR4FromR8", 71, 14864u, NULL},
  {"VarR4FromCy", 72, 435296u, NULL},
  {"VarR4FromDate", 73, 380784u, NULL},
  {"VarR4FromStr", 74, 9824u, NULL},
  {"VarR4FromDisp", 75, 380800u, NULL},
  {"VarR4FromBool", 76, 380752u, NULL},
  {"SafeArrayGetVartype", 77, 20944u, NULL},
  {"VarR8FromUI1", 78, 378400u, NULL},
  {"VarR8FromI2", 79, 377920u, NULL},
  {"VarR8FromI4", 80, 381296u, NULL},
  {"VarR8FromR4", 81, 381360u, NULL},
  {"VarR8FromCy", 82, 435344u, NULL},
  {"VarR8FromDate", 83, 381152u, NULL},
  {"VarR8FromStr", 84, 104960u, NULL},
  {"VarR8FromDisp", 85, 381168u, NULL},
  {"VarR8FromBool", 86, 377920u, NULL},
  {"VarFormat", 87, 441248u, NULL},
  {"VarDateFromUI1", 88, 378400u, NULL},
  {"VarDateFromI2", 89, 377920u, NULL},
  {"VarDateFromI4", 90, 378176u, NULL},
  {"VarDateFromR4", 91, 378304u, NULL},
  {"VarDateFromR8", 92, 378352u, NULL},
  {"VarDateFromCy", 93, 377952u, NULL},
  {"VarDateFromStr", 94, 432464u, NULL},
  {"VarDateFromDisp", 95, 378016u, NULL},
  {"VarDateFromBool", 96, 377920u, NULL},
  {"VarFormatDateTime", 97, 441776u, NULL},
  {"VarCyFromUI1", 98, 377776u, NULL},
  {"VarCyFromI2", 99, 434720u, NULL},
  {"VarCyFromI4", 100, 434752u, NULL},
  {"VarCyFromR4", 101, 434784u, NULL},
  {"VarCyFromR8", 102, 434800u, NULL},
  {"VarCyFromDate", 103, 434800u, NULL},
  {"VarCyFromStr", 104, 377632u, NULL},
  {"VarCyFromDisp", 105, 377520u, NULL},
  {"VarCyFromBool", 106, 434720u, NULL},
  {"VarFormatNumber", 107, 450176u, NULL},
  {"VarBstrFromUI1", 108, 377488u, NULL},
  {"VarBstrFromI2", 109, 377008u, NULL},
  {"VarBstrFromI4", 110, 88528u, NULL},
  {"VarBstrFromR4", 111, 377184u, NULL},
  {"VarBstrFromR8", 112, 102192u, NULL},
  {"VarBstrFromCy", 113, 375936u, NULL},
  {"VarBstrFromDate", 114, 116256u, NULL},
  {"VarBstrFromDisp", 115, 376912u, NULL},
  {"VarBstrFromBool", 116, 102832u, NULL},
  {"VarFormatPercent", 117, 450448u, NULL},
  {"VarBoolFromUI1", 118, 375744u, NULL},
  {"VarBoolFromI2", 119, 375776u, NULL},
  {"VarBoolFromI4", 120, 375808u, NULL},
  {"VarBoolFromR4", 121, 375872u, NULL},
  {"VarBoolFromR8", 122, 375904u, NULL},
  {"VarBoolFromDate", 123, 375648u, NULL},
  {"VarBoolFromCy", 124, 375616u, NULL},
  {"VarBoolFromStr", 125, 102944u, NULL},
  {"VarBoolFromDisp", 126, 375664u, NULL},
  {"VarFormatCurrency", 127, 441488u, NULL},
  {"VarWeekdayName", 128, 451568u, NULL},
  {"VarMonthName", 129, 450704u, NULL},
  {"VarUI1FromI2", 130, 381712u, NULL},
  {"VarUI1FromI4", 131, 381744u, NULL},
  {"VarUI1FromR4", 132, 381808u, NULL},
  {"VarUI1FromR8", 133, 381824u, NULL},
  {"VarUI1FromCy", 134, 381520u, NULL},
  {"VarUI1FromDate", 135, 381584u, NULL},
  {"VarUI1FromStr", 136, 381888u, NULL},
  {"VarUI1FromDisp", 137, 381600u, NULL},
  {"VarUI1FromBool", 138, 378656u, NULL},
  {"VarFormatFromTokens", 139, 441920u, NULL},
  {"VarTokenizeFormatString", 140, 450912u, NULL},
  {"VarAdd", 141, 350400u, NULL},
  {"VarAnd", 142, 360896u, NULL},
  {"VarDiv", 143, 353328u, NULL},
  {"BSTR_UserFree64", 144, 87984u, NULL},
  {"BSTR_UserMarshal64", 145, 25136u, NULL},
  {"DispCallFunc", 146, 75664u, NULL},
  {"VariantChangeTypeEx", 147, 70576u, NULL},
  {"SafeArrayPtrOfIndex", 148, 25264u, NULL},
  {"SysStringByteLen", 149, 23088u, NULL},
  {"SysAllocStringByteLen", 150, 69680u, NULL},
  {"BSTR_UserSize64", 151, 26224u, NULL},
  {"VarEqv", 152, 361296u, NULL},
  {"VarIdiv", 153, 361344u, NULL},
  {"VarImp", 154, 361792u, NULL},
  {"VarMod", 155, 362032u, NULL},
  {"VarMul", 156, 355008u, NULL},
  {"VarOr", 157, 362592u, NULL},
  {"VarPow", 158, 357296u, NULL},
  {"VarSub", 159, 357888u, NULL},
  {"CreateTypeLib", 160, 201136u, NULL},
  {"LoadTypeLib", 161, 31808u, NULL},
  {"LoadRegTypeLib", 162, 97152u, NULL},
  {"RegisterTypeLib", 163, 211952u, NULL},
  {"QueryPathOfRegTypeLib", 164, 211680u, NULL},
  {"LHashValOfNameSys", 165, 30368u, NULL},
  {"LHashValOfNameSysA", 166, 78784u, NULL},
  {"VarXor", 167, 363008u, NULL},
  {"VarAbs", 168, 346816u, NULL},
  {"VarFix", 169, 347552u, NULL},
  {"OaBuildVersion", 170, 218528u, NULL},
  {"ClearCustData", 171, 305568u, NULL},
  {"VarInt", 172, 348064u, NULL},
  {"VarNeg", 173, 348544u, NULL},
  {"VarNot", 174, 362432u, NULL},
  {"VarRound", 175, 349456u, NULL},
  {"VarCmp", 176, 26656u, NULL},
  {"VarDecAdd", 177, 366432u, NULL},
  {"VarDecDiv", 178, 366448u, NULL},
  {"VarDecMul", 179, 368160u, NULL},
  {"CreateTypeLib2", 180, 305712u, NULL},
  {"VarDecSub", 181, 368832u, NULL},
  {"VarDecAbs", 182, 366384u, NULL},
  {"LoadTypeLibEx", 183, 45472u, NULL},
  {"SystemTimeToVariantTime", 184, 108656u, NULL},
  {"VariantTimeToSystemTime", 185, 109552u, NULL},
  {"UnRegisterTypeLib", 186, 203344u, NULL},
  {"VarDecFix", 187, 9312u, NULL},
  {"VarDecInt", 188, 368096u, NULL},
  {"VarDecNeg", 189, 368784u, NULL},
  {"VarDecFromUI1", 190, 419440u, NULL},
  {"VarDecFromI2", 191, 419136u, NULL},
  {"VarDecFromI4", 192, 13920u, NULL},
  {"VarDecFromR4", 193, 6512u, NULL},
  {"VarDecFromR8", 194, 5760u, NULL},
  {"VarDecFromDate", 195, 418976u, NULL},
  {"VarDecFromCy", 196, 418928u, NULL},
  {"VarDecFromStr", 197, 419280u, NULL},
  {"VarDecFromDisp", 198, 418992u, NULL},
  {"VarDecFromBool", 199, 418880u, NULL},
  {"GetErrorInfo", 200, 126576u, NULL},
  {"SetErrorInfo", 201, 126592u, NULL},
  {"CreateErrorInfo", 202, 126560u, NULL},
  {"VarDecRound", 203, 6160u, NULL},
  {"VarDecCmp", 204, 23328u, NULL},
  {"VarI2FromI1", 205, 379344u, NULL},
  {"VarI2FromUI2", 206, 379696u, NULL},
  {"VarI2FromUI4", 207, 379728u, NULL},
  {"VarI2FromDec", 208, 419696u, NULL},
  {"VarI4FromI1", 209, 379904u, NULL},
  {"VarI4FromUI2", 210, 380000u, NULL},
  {"VarI4FromUI4", 211, 380016u, NULL},
  {"VarI4FromDec", 212, 419840u, NULL},
  {"VarR4FromI1", 213, 380896u, NULL},
  {"VarR4FromUI2", 214, 381024u, NULL},
  {"VarR4FromUI4", 215, 381056u, NULL},
  {"VarR4FromDec", 216, 420144u, NULL},
  {"VarR8FromI1", 217, 381264u, NULL},
  {"VarR8FromUI2", 218, 381392u, NULL},
  {"VarR8FromUI4", 219, 381424u, NULL},
  {"VarR8FromDec", 220, 112768u, NULL},
  {"VarDateFromI1", 221, 378112u, NULL},
  {"VarDateFromUI2", 222, 378432u, NULL},
  {"VarDateFromUI4", 223, 378496u, NULL},
  {"VarDateFromDec", 224, 418784u, NULL},
  {"VarCyFromI1", 225, 377600u, NULL},
  {"VarCyFromUI2", 226, 377808u, NULL},
  {"VarCyFromUI4", 227, 377840u, NULL},
  {"VarCyFromDec", 228, 417952u, NULL},
  {"VarBstrFromI1", 229, 376992u, NULL},
  {"VarBstrFromUI2", 230, 377504u, NULL},
  {"VarBstrFromUI4", 231, 102368u, NULL},
  {"VarBstrFromDec", 232, 5088u, NULL},
  {"VarBoolFromI1", 233, 375744u, NULL},
  {"VarBoolFromUI2", 234, 375776u, NULL},
  {"VarBoolFromUI4", 235, 375808u, NULL},
  {"VarBoolFromDec", 236, 417904u, NULL},
  {"VarUI1FromI1", 237, 381680u, NULL},
  {"VarUI1FromUI2", 238, 381712u, NULL},
  {"VarUI1FromUI4", 239, 381744u, NULL},
  {"VarUI1FromDec", 240, 420224u, NULL},
  {"VarDecFromI1", 241, 419088u, NULL},
  {"VarDecFromUI2", 242, 419472u, NULL},
  {"VarDecFromUI4", 243, 419504u, NULL},
  {"VarI1FromUI1", 244, 379104u, NULL},
  {"VarI1FromI2", 245, 378832u, NULL},
  {"VarI1FromI4", 246, 378880u, NULL},
  {"VarI1FromR4", 247, 378960u, NULL},
  {"VarI1FromR8", 248, 378976u, NULL},
  {"VarI1FromDate", 249, 378736u, NULL},
  {"VarI1FromCy", 250, 378672u, NULL},
  {"VarI1FromStr", 251, 379040u, NULL},
  {"VarI1FromDisp", 252, 378752u, NULL},
  {"VarI1FromBool", 253, 378656u, NULL},
  {"VarI1FromUI2", 254, 379136u, NULL},
  {"VarI1FromUI4", 255, 379168u, NULL},
  {"VarI1FromDec", 256, 419568u, NULL},
  {"VarUI2FromUI1", 257, 379680u, NULL},
  {"VarUI2FromI2", 258, 382144u, NULL},
  {"VarUI2FromI4", 259, 382176u, NULL},
  {"VarUI2FromR4", 260, 382240u, NULL},
  {"VarUI2FromR8", 261, 382256u, NULL},
  {"VarUI2FromDate", 262, 382016u, NULL},
  {"VarUI2FromCy", 263, 381952u, NULL},
  {"VarUI2FromStr", 264, 382320u, NULL},
  {"VarUI2FromDisp", 265, 382032u, NULL},
  {"VarUI2FromBool", 266, 379232u, NULL},
  {"VarUI2FromI1", 267, 382112u, NULL},
  {"VarUI2FromUI4", 268, 382176u, NULL},
  {"VarUI2FromDec", 269, 420352u, NULL},
  {"VarUI4FromUI1", 270, 379984u, NULL},
  {"VarUI4FromI2", 271, 382544u, NULL},
  {"VarUI4FromI4", 272, 382576u, NULL},
  {"VarUI4FromR4", 273, 382640u, NULL},
  {"VarUI4FromR8", 274, 382656u, NULL},
  {"VarUI4FromDate", 275, 382416u, NULL},
  {"VarUI4FromCy", 276, 382384u, NULL},
  {"VarUI4FromStr", 277, 382720u, NULL},
  {"VarUI4FromDisp", 278, 382432u, NULL},
  {"VarUI4FromBool", 279, 379792u, NULL},
  {"VarUI4FromI1", 280, 382512u, NULL},
  {"VarUI4FromUI2", 281, 380000u, NULL},
  {"VarUI4FromDec", 282, 8992u, NULL},
  {"BSTR_UserSize", 283, 19680u, NULL},
  {"BSTR_UserMarshal", 284, 19552u, NULL},
  {"BSTR_UserUnmarshal", 285, 84560u, NULL},
  {"BSTR_UserFree", 286, 81024u, NULL},
  {"VARIANT_UserSize", 287, 16704u, NULL},
  {"VARIANT_UserMarshal", 288, 17536u, NULL},
  {"VARIANT_UserUnmarshal", 289, 81664u, NULL},
  {"VARIANT_UserFree", 290, 22016u, NULL},
  {"LPSAFEARRAY_UserSize", 291, 374224u, NULL},
  {"LPSAFEARRAY_UserMarshal", 292, 374208u, NULL},
  {"LPSAFEARRAY_UserUnmarshal", 293, 374240u, NULL},
  {"LPSAFEARRAY_UserFree", 294, 22208u, NULL},
  {"LPSAFEARRAY_Size", 295, 18080u, NULL},
  {"LPSAFEARRAY_Marshal", 296, 18896u, NULL},
  {"LPSAFEARRAY_Unmarshal", 297, 97200u, NULL},
  {"VarDecCmpR8", 298, 363872u, NULL},
  {"VarCyAdd", 299, 364112u, NULL},
  {"BSTR_UserUnmarshal64", 300, 88000u, NULL},
  {"DllCanUnloadNow", 301, 30672u, NULL},
  {"DllGetClassObject", 302, 42368u, NULL},
  {"VarCyMul", 303, 364464u, NULL},
  {"VarCyMulI4", 304, 364800u, NULL},
  {"VarCySub", 305, 364960u, NULL},
  {"VarCyAbs", 306, 364064u, NULL},
  {"VarCyFix", 307, 364288u, NULL},
  {"VarCyInt", 308, 364352u, NULL},
  {"VarCyNeg", 309, 364912u, NULL},
  {"VarCyRound", 310, 347424u, NULL},
  {"VarCyCmp", 311, 364160u, NULL},
  {"VarCyCmpR8", 312, 364192u, NULL},
  {"VarBstrCat", 313, 28944u, NULL},
  {"VarBstrCmp", 314, 15712u, NULL},
  {"VarR8Pow", 315, 357664u, NULL},
  {"VarR4CmpR8", 316, 363984u, NULL},
  {"VarR8Round", 317, 349216u, NULL},
  {"VarCat", 318, 352960u, NULL},
  {"VarDateFromUdateEx", 319, 108928u, NULL},
  {"DllRegisterServer", 320, 389648u, NULL},
  {"DllUnregisterServer", 321, 390480u, NULL},
  {"GetRecordInfoFromGuids", 322, 411808u, NULL},
  {"GetRecordInfoFromTypeInfo", 323, 412192u, NULL},
  {"LPSAFEARRAY_UserFree64", 324, 374192u, NULL},
  {"SetVarConversionLocaleSetting", 325, 217280u, NULL},
  {"GetVarConversionLocaleSetting", 326, 217168u, NULL},
  {"SetOaNoCache", 327, 217248u, NULL},
  {"LPSAFEARRAY_UserMarshal64", 328, 12384u, NULL},
  {"VarCyMulI8", 329, 353184u, NULL},
  {"VarDateFromUdate", 330, 13888u, NULL},
  {"VarUdateFromDate", 331, 109680u, NULL},
  {"GetAltMonthNames", 332, 391904u, NULL},
  {"VarI8FromUI1", 333, 380656u, NULL},
  {"VarI8FromI2", 334, 380080u, NULL},
  {"VarI8FromR4", 335, 380240u, NULL},
  {"VarI8FromR8", 336, 380256u, NULL},
  {"VarI8FromCy", 337, 435440u, NULL},
  {"VarI8FromDate", 338, 380112u, NULL},
  {"VarI8FromStr", 339, 380320u, NULL},
  {"VarI8FromDisp", 340, 380128u, NULL},
  {"VarI8FromBool", 341, 380080u, NULL},
  {"VarI8FromI1", 342, 380208u, NULL},
  {"VarI8FromUI2", 343, 380672u, NULL},
  {"VarI8FromUI4", 344, 380688u, NULL},
  {"VarI8FromDec", 345, 419984u, NULL},
  {"VarI2FromI8", 346, 379408u, NULL},
  {"VarI2FromUI8", 347, 379760u, NULL},
  {"VarI4FromI8", 348, 379920u, NULL},
  {"VarI4FromUI8", 349, 380048u, NULL},
  {"LPSAFEARRAY_UserSize64", 350, 13040u, NULL},
  {"LPSAFEARRAY_UserUnmarshal64", 351, 9968u, NULL},
  {"OACreateTypeLib2", 352, 305712u, NULL},
  {"SafeArrayAddRef", 353, 416592u, NULL},
  {"SafeArrayReleaseData", 354, 417568u, NULL},
  {"SafeArrayReleaseDescriptor", 355, 417584u, NULL},
  {"SysAddRefString", 356, 13024u, NULL},
  {"SysReleaseString", 357, 88480u, NULL},
  {"VARIANT_UserFree64", 358, 22000u, NULL},
  {"VARIANT_UserMarshal64", 359, 24640u, NULL},
  {"VarR4FromI8", 360, 380960u, NULL},
  {"VarR4FromUI8", 361, 381088u, NULL},
  {"VarR8FromI8", 362, 381328u, NULL},
  {"VarR8FromUI8", 363, 381456u, NULL},
  {"VarDateFromI8", 364, 378240u, NULL},
  {"VarDateFromUI8", 365, 378560u, NULL},
  {"VarCyFromI8", 366, 435376u, NULL},
  {"VarCyFromUI8", 367, 377872u, NULL},
  {"VarBstrFromI8", 368, 9424u, NULL},
  {"VarBstrFromUI8", 369, 112960u, NULL},
  {"VarBoolFromI8", 370, 375840u, NULL},
  {"VarBoolFromUI8", 371, 375840u, NULL},
  {"VarUI1FromI8", 372, 381776u, NULL},
  {"VarUI1FromUI8", 373, 381776u, NULL},
  {"VarDecFromI8", 374, 419200u, NULL},
  {"VarDecFromUI8", 375, 419536u, NULL},
  {"VarI1FromI8", 376, 378912u, NULL},
  {"VarI1FromUI8", 377, 379200u, NULL},
  {"VarUI2FromI8", 378, 382208u, NULL},
  {"VarUI2FromUI8", 379, 382208u, NULL},
  {"VARIANT_UserSize64", 380, 25408u, NULL},
  {"VARIANT_UserUnmarshal64", 381, 90512u, NULL},
  {"OleLoadPictureEx", 401, 415568u, NULL},
  {"OleLoadPictureFileEx", 402, 415776u, NULL},
  {"SafeArrayCreateVector", 411, 89504u, NULL},
  {"SafeArrayCopyData", 412, 87136u, NULL},
  {"VectorFromBstr", 413, 383312u, NULL},
  {"BstrFromVector", 414, 374880u, NULL},
  {"OleIconToCursor", 415, 415424u, NULL},
  {"OleCreatePropertyFrameIndirect", 416, 415360u, NULL},
  {"OleCreatePropertyFrame", 417, 415264u, NULL},
  {"OleLoadPicture", 418, 415520u, NULL},
  {"OleCreatePictureIndirect", 419, 14128u, NULL},
  {"OleCreateFontIndirect", 420, 14240u, NULL},
  {"OleTranslateColor", 421, 120000u, NULL},
  {"OleLoadPictureFile", 422, 415712u, NULL},
  {"OleSavePictureFile", 423, 416048u, NULL},
  {"OleLoadPicturePath", 424, 415920u, NULL},
  {"VarUI4FromI8", 425, 382608u, NULL},
  {"VarUI4FromUI8", 426, 382608u, NULL},
  {"VarI8FromUI8", 427, 380704u, NULL},
  {"VarUI8FromI8", 428, 383056u, NULL},
  {"VarUI8FromUI1", 429, 380656u, NULL},
  {"VarUI8FromI2", 430, 383024u, NULL},
  {"VarUI8FromR4", 431, 383088u, NULL},
  {"VarUI8FromR8", 432, 383104u, NULL},
  {"VarUI8FromCy", 433, 382864u, NULL},
  {"VarUI8FromDate", 434, 382896u, NULL},
  {"VarUI8FromStr", 435, 383168u, NULL},
  {"VarUI8FromDisp", 436, 382912u, NULL},
  {"VarUI8FromBool", 437, 380080u, NULL},
  {"VarUI8FromI1", 438, 382992u, NULL},
  {"VarUI8FromUI2", 439, 380672u, NULL},
  {"VarUI8FromUI4", 440, 380688u, NULL},
  {"VarUI8FromDec", 441, 420480u, NULL},
  {"RegisterTypeLibForUser", 442, 212128u, NULL},
  {"UnRegisterTypeLibForUser", 443, 203488u, NULL},
  {"OaEnablePerUserTLibRegistration", 444, 217216u, NULL},
  {"HWND_UserFree", 445, 753257u, "COMBASE.HWND_UserFree"},
  {"HWND_UserMarshal", 446, 753336u, "COMBASE.HWND_UserMarshal"},
  {"HWND_UserSize", 447, 753421u, "COMBASE.HWND_UserSize"},
  {"HWND_UserUnmarshal", 448, 753502u, "COMBASE.HWND_UserUnmarshal"},
  {"HWND_UserFree64", 449, 753295u, "COMBASE.HWND_UserFree64"},
  {"HWND_UserMarshal64", 450, 753380u, "COMBASE.HWND_UserMarshal64"},
  {"HWND_UserSize64", 451, 753459u, "COMBASE.HWND_UserSize64"},
  {"HWND_UserUnmarshal64", 452, 753550u, "COMBASE.HWND_UserUnmarshal64"},
  {"OACleanup", 500, 19920u, NULL},
  {NULL, 501, 210256u, NULL},
  {NULL, 502, 216736u, NULL},
};
#define N 427
#define DLL "oleaut32.dll"

TEST(exp_oleaut32_resolve_00) { /* ordinals 2..17 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 0, 16);
}
TEST(exp_oleaut32_resolve_01) { /* ordinals 18..33 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 16, 32);
}
TEST(exp_oleaut32_resolve_02) { /* ordinals 34..49 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 32, 48);
}
TEST(exp_oleaut32_resolve_03) { /* ordinals 50..65 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 48, 64);
}
TEST(exp_oleaut32_resolve_04) { /* ordinals 66..81 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 64, 80);
}
TEST(exp_oleaut32_resolve_05) { /* ordinals 82..97 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 80, 96);
}
TEST(exp_oleaut32_resolve_06) { /* ordinals 98..113 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 96, 112);
}
TEST(exp_oleaut32_resolve_07) { /* ordinals 114..129 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 112, 128);
}
TEST(exp_oleaut32_resolve_08) { /* ordinals 130..145 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 128, 144);
}
TEST(exp_oleaut32_resolve_09) { /* ordinals 146..161 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 144, 160);
}
TEST(exp_oleaut32_resolve_10) { /* ordinals 162..177 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 160, 176);
}
TEST(exp_oleaut32_resolve_11) { /* ordinals 178..193 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 176, 192);
}
TEST(exp_oleaut32_resolve_12) { /* ordinals 194..209 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 192, 208);
}
TEST(exp_oleaut32_resolve_13) { /* ordinals 210..225 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 208, 224);
}
TEST(exp_oleaut32_resolve_14) { /* ordinals 226..241 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 224, 240);
}
TEST(exp_oleaut32_resolve_15) { /* ordinals 242..257 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 240, 256);
}
TEST(exp_oleaut32_resolve_16) { /* ordinals 258..273 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 256, 272);
}
TEST(exp_oleaut32_resolve_17) { /* ordinals 274..289 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 272, 288);
}
TEST(exp_oleaut32_resolve_18) { /* ordinals 290..305 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 288, 304);
}
TEST(exp_oleaut32_resolve_19) { /* ordinals 306..321 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 304, 320);
}
TEST(exp_oleaut32_resolve_20) { /* ordinals 322..337 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 320, 336);
}
TEST(exp_oleaut32_resolve_21) { /* ordinals 338..353 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 336, 352);
}
TEST(exp_oleaut32_resolve_22) { /* ordinals 354..369 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 352, 368);
}
TEST(exp_oleaut32_resolve_23) { /* ordinals 370..412 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 368, 384);
}
TEST(exp_oleaut32_resolve_24) { /* ordinals 413..428 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 384, 400);
}
TEST(exp_oleaut32_resolve_25) { /* ordinals 429..444 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 400, 416);
}
TEST(exp_oleaut32_resolve_26) { /* ordinals 445..502 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 416, 427);
}
TEST(exp_oleaut32_export_directory) { REQUIRE(t_module(DLL)); exp_check_dir(DLL, tbl, N, 2, 502, 425); }
TEST(exp_oleaut32_alias_groups) { REQUIRE(t_module(DLL)); exp_check_alias_groups(DLL, tbl, N); }
TEST(exp_oleaut32_gap_ordinals) { REQUIRE(t_module(DLL)); exp_check_gaps(DLL, tbl, N, 2, 502); }
TEST(exp_oleaut32_negative_lookups) { REQUIRE(t_module(DLL)); exp_check_negatives(DLL, "SysAllocString", 502); }
TEST(exp_oleaut32_name_count_and_unique) {
    int names = 0;
    for (int i = 0; i < N; i++) { if (tbl[i].name) names++; for (int j = i + 1; j < N; j++) { CHECK(tbl[i].ord != tbl[j].ord); if (tbl[i].name && tbl[j].name) CHECK(strcmp(tbl[i].name, tbl[j].name)); } }
    EXPECT_EQ(names, 425); EXPECT_EQ(N, 427);
}
TEST(exp_oleaut32_forwarders) {
    REQUIRE(t_module(DLL));
    { FARPROC a = t_proc_in(DLL, "HWND_UserFree"), b = t_proc_in("combase.dll", "HWND_UserFree"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 445); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserMarshal"), b = t_proc_in("combase.dll", "HWND_UserMarshal"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 446); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserSize"), b = t_proc_in("combase.dll", "HWND_UserSize"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 447); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserUnmarshal"), b = t_proc_in("combase.dll", "HWND_UserUnmarshal"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 448); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserFree64"), b = t_proc_in("combase.dll", "HWND_UserFree64"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 449); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserMarshal64"), b = t_proc_in("combase.dll", "HWND_UserMarshal64"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 450); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserSize64"), b = t_proc_in("combase.dll", "HWND_UserSize64"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 451); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "HWND_UserUnmarshal64"), b = t_proc_in("combase.dll", "HWND_UserUnmarshal64"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 452); CHECK(c == a); }
}
TEST(exp_oleaut32_forwarder_target_in_other_image) {
    REQUIRE(t_module(DLL));
    for (int i = 0; i < N; i++) if (tbl[i].fwd) { FARPROC p = t_proc_in(DLL, tbl[i].name); HMODULE h = NULL; if (p) GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h); CHECK(h != NULL); }
}
TEST(exp_oleaut32_ordinal_only) {
    REQUIRE(t_module(DLL));
    { FARPROC p = t_proc_ord(DLL, 501); CHECK(p != NULL); CHECK(exp_in_image(t_module(DLL), p)); }
    { FARPROC p = t_proc_ord(DLL, 502); CHECK(p != NULL); CHECK(exp_in_image(t_module(DLL), p)); }
}
TEST(exp_oleaut32_stable_ordinals) {
    REQUIRE(t_module(DLL));
    CHECK(t_proc_in(DLL, "SysAllocString") != NULL); CHECK(t_proc_in(DLL, "SysAllocString") == t_proc_ord(DLL, 2));
    CHECK(t_proc_in(DLL, "SysReAllocString") != NULL); CHECK(t_proc_in(DLL, "SysReAllocString") == t_proc_ord(DLL, 3));
    CHECK(t_proc_in(DLL, "SysAllocStringLen") != NULL); CHECK(t_proc_in(DLL, "SysAllocStringLen") == t_proc_ord(DLL, 4));
    CHECK(t_proc_in(DLL, "SysReAllocStringLen") != NULL); CHECK(t_proc_in(DLL, "SysReAllocStringLen") == t_proc_ord(DLL, 5));
    CHECK(t_proc_in(DLL, "SysFreeString") != NULL); CHECK(t_proc_in(DLL, "SysFreeString") == t_proc_ord(DLL, 6));
    CHECK(t_proc_in(DLL, "SysStringLen") != NULL); CHECK(t_proc_in(DLL, "SysStringLen") == t_proc_ord(DLL, 7));
    CHECK(t_proc_in(DLL, "VariantInit") != NULL); CHECK(t_proc_in(DLL, "VariantInit") == t_proc_ord(DLL, 8));
    CHECK(t_proc_in(DLL, "VariantClear") != NULL); CHECK(t_proc_in(DLL, "VariantClear") == t_proc_ord(DLL, 9));
    CHECK(t_proc_in(DLL, "VariantCopy") != NULL); CHECK(t_proc_in(DLL, "VariantCopy") == t_proc_ord(DLL, 10));
    CHECK(t_proc_in(DLL, "VariantCopyInd") != NULL); CHECK(t_proc_in(DLL, "VariantCopyInd") == t_proc_ord(DLL, 11));
    CHECK(t_proc_in(DLL, "VariantChangeType") != NULL); CHECK(t_proc_in(DLL, "VariantChangeType") == t_proc_ord(DLL, 12));
    CHECK(t_proc_in(DLL, "VariantTimeToDosDateTime") != NULL); CHECK(t_proc_in(DLL, "VariantTimeToDosDateTime") == t_proc_ord(DLL, 13));
    CHECK(t_proc_in(DLL, "DosDateTimeToVariantTime") != NULL); CHECK(t_proc_in(DLL, "DosDateTimeToVariantTime") == t_proc_ord(DLL, 14));
    CHECK(t_proc_in(DLL, "SafeArrayCreate") != NULL); CHECK(t_proc_in(DLL, "SafeArrayCreate") == t_proc_ord(DLL, 15));
    CHECK(t_proc_in(DLL, "SafeArrayDestroy") != NULL); CHECK(t_proc_in(DLL, "SafeArrayDestroy") == t_proc_ord(DLL, 16));
    CHECK(t_proc_in(DLL, "SafeArrayGetDim") != NULL); CHECK(t_proc_in(DLL, "SafeArrayGetDim") == t_proc_ord(DLL, 17));
    CHECK(t_proc_in(DLL, "SafeArrayGetElemsize") != NULL); CHECK(t_proc_in(DLL, "SafeArrayGetElemsize") == t_proc_ord(DLL, 18));
    CHECK(t_proc_in(DLL, "SafeArrayGetUBound") != NULL); CHECK(t_proc_in(DLL, "SafeArrayGetUBound") == t_proc_ord(DLL, 19));
    CHECK(t_proc_in(DLL, "SafeArrayGetLBound") != NULL); CHECK(t_proc_in(DLL, "SafeArrayGetLBound") == t_proc_ord(DLL, 20));
    CHECK(t_proc_in(DLL, "SafeArrayLock") != NULL); CHECK(t_proc_in(DLL, "SafeArrayLock") == t_proc_ord(DLL, 21));
    CHECK(t_proc_in(DLL, "SafeArrayUnlock") != NULL); CHECK(t_proc_in(DLL, "SafeArrayUnlock") == t_proc_ord(DLL, 22));
    CHECK(t_proc_in(DLL, "SafeArrayAccessData") != NULL); CHECK(t_proc_in(DLL, "SafeArrayAccessData") == t_proc_ord(DLL, 23));
    CHECK(t_proc_in(DLL, "SafeArrayUnaccessData") != NULL); CHECK(t_proc_in(DLL, "SafeArrayUnaccessData") == t_proc_ord(DLL, 24));
    CHECK(t_proc_in(DLL, "SafeArrayGetElement") != NULL); CHECK(t_proc_in(DLL, "SafeArrayGetElement") == t_proc_ord(DLL, 25));
    CHECK(t_proc_in(DLL, "SafeArrayPutElement") != NULL); CHECK(t_proc_in(DLL, "SafeArrayPutElement") == t_proc_ord(DLL, 26));
    CHECK(t_proc_in(DLL, "SafeArrayCopy") != NULL); CHECK(t_proc_in(DLL, "SafeArrayCopy") == t_proc_ord(DLL, 27));
}
TEST(exp_oleaut32_sysstring_roundtrip_via_ordinals) {
    typedef BSTR (WINAPI *alloc_t)(const OLECHAR *); typedef UINT (WINAPI *len_t)(BSTR); typedef void (WINAPI *free_t)(BSTR);
    alloc_t a = (alloc_t)t_proc_ord(DLL, 2); len_t l = (len_t)t_proc_ord(DLL, 7); free_t f = (free_t)t_proc_ord(DLL, 6);
    REQUIRE(a); REQUIRE(l); REQUIRE(f);
    BSTR b = a(L"abc"); CHECK(b != NULL);
    if (b) { EXPECT_EQ(l(b), 3); f(b); }
}
