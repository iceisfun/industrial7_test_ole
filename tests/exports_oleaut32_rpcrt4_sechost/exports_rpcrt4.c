#include "exp.h"

static const exp_ent tbl[] = {
  {NULL, 1000, 411728u, NULL},
  {"I_RpcBindingInqCurrentModifiedId", 1001, 363408u, NULL},
  {"I_RpcFixTransferSyntax", 1002, 677232u, NULL},
  {"I_RpcFwThisIsTheManager", 1003, 677744u, NULL},
  {"I_RpcInitFwImports", 1004, 428176u, NULL},
  {"I_RpcInitHttpImports", 1005, 867840u, NULL},
  {"I_RpcInitImports", 1006, 428880u, NULL},
  {"I_RpcInitNdrImports", 1007, 415424u, NULL},
  {"I_RpcMgmtQueryDedicatedThreadPool", 1008, 417888u, NULL},
  {"I_RpcOpenClientThread", 1009, 16400u, NULL},
  {"I_RpcServerTurnOnOffKeepalives", 1010, 678288u, NULL},
  {"I_RpcVerifierCorruptionExpected", 1011, 682464u, NULL},
  {"NdrFullPointerFree", 1012, 347744u, NULL},
  {"NdrFullPointerInsertRefId", 1013, 72016u, NULL},
  {"NdrFullPointerQueryPointer", 1014, 71520u, NULL},
  {"NdrFullPointerQueryRefId", 1015, 72992u, NULL},
  {"NdrpClientCall2", 1016, 4592u, NULL},
  {"RpcCertMatchPrincipalName", 1017, 683264u, NULL},
  {"pfnFreeRoutines", 1018, 975912u, NULL},
  {"pfnMarshallRoutines", 1019, 975920u, NULL},
  {"pfnSizeRoutines", 1020, 975928u, NULL},
  {"pfnUnmarshallRoutines", 1021, 975936u, NULL},
  {"CStdStubBuffer_AddRef", 1022, 372688u, NULL},
  {"CStdStubBuffer_Connect", 1023, 823712u, NULL},
  {"CStdStubBuffer_CountRefs", 1024, 823808u, NULL},
  {"CStdStubBuffer_DebugServerQueryInterface", 1025, 823888u, NULL},
  {"CStdStubBuffer_DebugServerRelease", 1026, 823984u, NULL},
  {"CStdStubBuffer_Disconnect", 1027, 384048u, NULL},
  {"CStdStubBuffer_Invoke", 1028, 369936u, NULL},
  {"CStdStubBuffer_IsIIDSupported", 1029, 824064u, NULL},
  {"CStdStubBuffer_QueryInterface", 1030, 398320u, NULL},
  {"CreateProxyFromTypeInfo", 1031, 402688u, NULL},
  {"CreateStubFromTypeInfo", 1032, 411424u, NULL},
  {"DceErrorInqTextA", 1033, 683904u, NULL},
  {"DceErrorInqTextW", 1034, 684048u, NULL},
  {"DllGetClassObject", 1035, 824160u, NULL},
  {"DllRegisterServer", 1036, 824272u, NULL},
  {"IUnknown_AddRef_Proxy", 1037, 368512u, NULL},
  {"IUnknown_QueryInterface_Proxy", 1038, 372864u, NULL},
  {"IUnknown_Release_Proxy", 1039, 368448u, NULL},
  {"I_RpcAllocate", 1040, 72848u, NULL},
  {"I_RpcAsyncAbortCall", 1041, 423824u, NULL},
  {"I_RpcAsyncSetHandle", 1042, 72176u, NULL},
  {"I_RpcBCacheAllocate", 1043, 301200u, NULL},
  {"I_RpcBCacheFree", 1044, 296848u, NULL},
  {"I_RpcBindingCopy", 1045, 684752u, NULL},
  {"I_RpcBindingCreateNP", 1046, 684864u, NULL},
  {"I_RpcBindingHandleToAsyncHandle", 1047, 715424u, NULL},
  {"I_RpcBindingInqClientTokenAttributes", 1048, 301600u, NULL},
  {"I_RpcBindingInqDynamicEndpoint", 1049, 1082089u, "RPCRT4.I_RpcBindingInqDynamicEndpointW"},
  {"I_RpcBindingInqDynamicEndpointA", 1050, 440384u, NULL},
  {"I_RpcBindingInqDynamicEndpointW", 1051, 46208u, NULL},
  {"I_RpcBindingInqLocalClientPID", 1052, 356848u, NULL},
  {"I_RpcBindingInqMarshalledTargetInfo", 1053, 715536u, NULL},
  {"I_RpcBindingInqSecurityContext", 1054, 685488u, NULL},
  {"I_RpcBindingInqSecurityContextKeyInfo", 1055, 685584u, NULL},
  {"I_RpcBindingInqTransportType", 1056, 362016u, NULL},
  {"I_RpcBindingInqWireIdForSnego", 1057, 715664u, NULL},
  {"I_RpcBindingIsClientLocal", 1058, 309456u, NULL},
  {"I_RpcBindingIsServerLocal", 1059, 678480u, NULL},
  {"I_RpcBindingSetPrivateOption", 1060, 418816u, NULL},
  {"I_RpcBindingToStaticStringBindingW", 1061, 716032u, NULL},
  {"I_RpcCertProcessAndProvision", 1062, 683488u, NULL},
  {"I_RpcClearMutex", 1063, 205408u, NULL},
  {"I_RpcCompleteAndFree", 1064, 677392u, NULL},
  {"I_RpcDeleteMutex", 1065, 716432u, NULL},
  {"I_RpcEnableWmiTrace", 1066, 683232u, NULL},
  {"I_RpcExceptionFilter", 1067, 364368u, NULL},
  {"I_RpcFilterDCOMActivation", 1068, 728736u, NULL},
  {"I_RpcFree", 1069, 354032u, NULL},
  {"I_RpcFreeBuffer", 1070, 369552u, NULL},
  {"I_RpcFreePipeBuffer", 1071, 729152u, NULL},
  {"I_RpcFreeSystemHandle", 1072, 394240u, NULL},
  {"I_RpcFreeSystemHandleCollection", 1073, 368352u, NULL},
  {"I_RpcGetBuffer", 1074, 323696u, NULL},
  {"I_RpcGetBufferWithObject", 1075, 323712u, NULL},
  {"I_RpcGetCurrentCallHandle", 1076, 729568u, NULL},
  {"I_RpcGetDefaultSD", 1077, 678592u, NULL},
  {"I_RpcGetExtendedError", 1078, 730464u, NULL},
  {"I_RpcGetPortAllocationData", 1079, 467840u, NULL},
  {"I_RpcGetSystemHandle", 1080, 388432u, NULL},
  {"I_RpcIfInqTransferSyntaxes", 1081, 46144u, NULL},
  {"I_RpcLogEvent", 1082, 229280u, NULL},
  {"I_RpcMapWin32Status", 1083, 197440u, NULL},
  {"I_RpcMarshalBindingHandleAndInterfaceForNDF", 1084, 685760u, NULL},
  {"I_RpcMgmtEnableDedicatedThreadPool", 1085, 678656u, NULL},
  {"I_RpcNDRCGetWireRepresentation", 1086, 806192u, NULL},
  {"I_RpcNDRSContextEmergencyCleanup", 1087, 807440u, NULL},
  {"I_RpcNegotiateTransferSyntax", 1088, 185072u, NULL},
  {"I_RpcNsBindingSetEntryName", 1089, 1083166u, "RPCRT4.I_RpcNsBindingSetEntryNameW"},
  {"I_RpcNsBindingSetEntryNameA", 1090, 440384u, NULL},
  {"I_RpcNsBindingSetEntryNameW", 1091, 685856u, NULL},
  {"I_RpcNsInterfaceExported", 1092, 685856u, NULL},
  {"I_RpcNsInterfaceUnexported", 1093, 685856u, NULL},
  {"I_RpcOpenClientProcess", 1094, 322992u, NULL},
  {"I_RpcParseSecurity", 1095, 257104u, NULL},
  {"I_RpcPauseExecution", 1096, 677664u, NULL},
  {"I_RpcReallocPipeBuffer", 1097, 729200u, NULL},
  {"I_RpcReceive", 1098, 304784u, NULL},
  {"I_RpcRecordCalloutFailure", 1099, 743232u, NULL},
  {"I_RpcRequestMutex", 1100, 377168u, NULL},
  {"I_RpcSNCHOption", 1101, 426544u, NULL},
  {"I_RpcSend", 1102, 301264u, NULL},
  {"I_RpcSendReceive", 1103, 197792u, NULL},
  {"I_RpcServerAllocateIpPort", 1104, 463344u, NULL},
  {"I_RpcServerCheckClientRestriction", 1105, 468288u, NULL},
  {"I_RpcServerDisableExceptionFilter", 1106, 428816u, NULL},
  {"I_RpcServerGetAssociationID", 1107, 324496u, NULL},
  {"I_RpcServerInqAddressChangeFn", 1108, 678704u, NULL},
  {"I_RpcServerInqLocalConnAddress", 1109, 313184u, NULL},
  {"I_RpcServerInqRemoteConnAddress", 1110, 313024u, NULL},
  {"I_RpcServerInqTransportType", 1111, 729616u, NULL},
  {"I_RpcServerIsClientDisconnected", 1112, 678720u, NULL},
  {"I_RpcServerRegisterForwardFunction", 1113, 468336u, NULL},
  {"I_RpcServerSetAddressChangeFn", 1114, 468432u, NULL},
  {"I_RpcServerStartService", 1115, 678864u, NULL},
  {"I_RpcServerSubscribeForDisconnectNotification", 1116, 440384u, NULL},
  {"I_RpcServerSubscribeForDisconnectNotification2", 1117, 410512u, NULL},
  {"I_RpcServerUnsubscribeForDisconnectNotification", 1118, 412752u, NULL},
  {"I_RpcServerUseProtseq2A", 1119, 445600u, NULL},
  {"I_RpcServerUseProtseq2W", 1120, 196096u, NULL},
  {"I_RpcServerUseProtseqEp2A", 1121, 465312u, NULL},
  {"I_RpcServerUseProtseqEp2W", 1122, 193232u, NULL},
  {"I_RpcSessionStrictContextHandle", 1123, 679040u, NULL},
  {"I_RpcSetDCOMAppId", 1124, 426400u, NULL},
  {"I_RpcSetSystemHandle", 1125, 391888u, NULL},
  {"I_RpcSsDontSerializeContext", 1126, 807472u, NULL},
  {"I_RpcSystemFunction001", 1127, 467648u, NULL},
  {"I_RpcSystemHandleTypeSpecificWork", 1128, 379200u, NULL},
  {"I_RpcTransConnectionAllocatePacket", 1129, 715792u, NULL},
  {"I_RpcTransConnectionFreePacket", 1130, 715808u, NULL},
  {"I_RpcTransConnectionReallocPacket", 1131, 424688u, NULL},
  {"I_RpcTransDatagramAllocate", 1132, 440384u, NULL},
  {"I_RpcTransDatagramAllocate2", 1133, 440384u, NULL},
  {"I_RpcTransDatagramFree", 1134, 440384u, NULL},
  {"I_RpcTransGetThreadEvent", 1135, 730528u, NULL},
  {"I_RpcTransGetThreadEventThreadOptional", 1136, 730560u, NULL},
  {"I_RpcTransIoCancelled", 1137, 715936u, NULL},
  {"I_RpcTransServerNewConnection", 1138, 728944u, NULL},
  {"I_RpcTurnOnEEInfoPropagation", 1139, 685952u, NULL},
  {"I_UuidCreate", 1140, 747760u, NULL},
  {"MesBufferHandleReset", 1141, 352032u, NULL},
  {"MesDecodeBufferHandleCreate", 1142, 352512u, NULL},
  {"MesDecodeIncrementalHandleCreate", 1143, 351440u, NULL},
  {"MesEncodeDynBufferHandleCreate", 1144, 351888u, NULL},
  {"MesEncodeFixedBufferHandleCreate", 1145, 352976u, NULL},
  {"MesEncodeIncrementalHandleCreate", 1146, 351280u, NULL},
  {"MesHandleFree", 1147, 361920u, NULL},
  {"MesIncrementalHandleReset", 1148, 351600u, NULL},
  {"MesInqProcEncodingId", 1149, 827632u, NULL},
  {"NDRCContextBinding", 1150, 185648u, NULL},
  {"NDRCContextMarshall", 1151, 326128u, NULL},
  {"NDRCContextUnmarshall", 1152, 806224u, NULL},
  {"NDRSContextMarshall", 1153, 807504u, NULL},
  {"NDRSContextMarshall2", 1154, 326288u, NULL},
  {"NDRSContextMarshallEx", 1155, 807616u, NULL},
  {"NDRSContextUnmarshall", 1156, 807648u, NULL},
  {"NDRSContextUnmarshall2", 1157, 86608u, NULL},
  {"NDRSContextUnmarshallEx", 1158, 807728u, NULL},
  {"Ndr64AsyncClientCall", 1159, 919008u, NULL},
  {"Ndr64AsyncServerCall64", 1160, 928624u, NULL},
  {"Ndr64AsyncServerCallAll", 1161, 901696u, NULL},
  {"Ndr64DcomAsyncClientCall", 1162, 824352u, NULL},
  {"Ndr64DcomAsyncStubCall", 1163, 824480u, NULL},
  {"NdrAllocate", 1164, 832672u, NULL},
  {"NdrAsyncClientCall", 1165, 6768u, NULL},
  {"NdrAsyncServerCall", 1166, 9424u, NULL},
  {"NdrByteCountPointerBufferSize", 1167, 181824u, NULL},
  {"NdrByteCountPointerFree", 1168, 399648u, NULL},
  {"NdrByteCountPointerMarshall", 1169, 395360u, NULL},
  {"NdrByteCountPointerUnmarshall", 1170, 387760u, NULL},
  {"NdrCStdStubBuffer2_Release", 1171, 375952u, NULL},
  {"NdrCStdStubBuffer_Release", 1172, 370032u, NULL},
  {"NdrClearOutParameters", 1173, 10800u, NULL},
  {"NdrClientCall2", 1174, 9376u, NULL},
  {"NdrClientCall3", 1175, 918720u, NULL},
  {"NdrClientContextMarshall", 1176, 811952u, NULL},
  {"NdrClientContextUnmarshall", 1177, 820832u, NULL},
  {"NdrClientInitialize", 1178, 186016u, NULL},
  {"NdrClientInitializeNew", 1179, 185952u, NULL},
  {"NdrComplexArrayBufferSize", 1180, 146288u, NULL},
  {"NdrComplexArrayFree", 1181, 136352u, NULL},
  {"NdrComplexArrayMarshall", 1182, 141664u, NULL},
  {"NdrComplexArrayMemorySize", 1183, 191184u, NULL},
  {"NdrComplexArrayUnmarshall", 1184, 165344u, NULL},
  {"NdrComplexStructBufferSize", 1185, 156816u, NULL},
  {"NdrComplexStructFree", 1186, 175152u, NULL},
  {"NdrComplexStructMarshall", 1187, 152656u, NULL},
  {"NdrComplexStructMemorySize", 1188, 160272u, NULL},
  {"NdrComplexStructUnmarshall", 1189, 148784u, NULL},
  {"NdrConformantArrayBufferSize", 1190, 139776u, NULL},
  {"NdrConformantArrayFree", 1191, 183520u, NULL},
  {"NdrConformantArrayMarshall", 1192, 139520u, NULL},
  {"NdrConformantArrayMemorySize", 1193, 189152u, NULL},
  {"NdrConformantArrayUnmarshall", 1194, 164080u, NULL},
  {"NdrConformantStringBufferSize", 1195, 180816u, NULL},
  {"NdrConformantStringMarshall", 1196, 355872u, NULL},
  {"NdrConformantStringMemorySize", 1197, 190720u, NULL},
  {"NdrConformantStringUnmarshall", 1198, 83296u, NULL},
  {"NdrConformantStructBufferSize", 1199, 180448u, NULL},
  {"NdrConformantStructFree", 1200, 810560u, NULL},
  {"NdrConformantStructMarshall", 1201, 355504u, NULL},
  {"NdrConformantStructMemorySize", 1202, 192144u, NULL},
  {"NdrConformantStructUnmarshall", 1203, 173760u, NULL},
  {"NdrConformantVaryingArrayBufferSize", 1204, 186832u, NULL},
  {"NdrConformantVaryingArrayFree", 1205, 411056u, NULL},
  {"NdrConformantVaryingArrayMarshall", 1206, 186496u, NULL},
  {"NdrConformantVaryingArrayMemorySize", 1207, 836928u, NULL},
  {"NdrConformantVaryingArrayUnmarshall", 1208, 188688u, NULL},
  {"NdrConformantVaryingStructBufferSize", 1209, 816128u, NULL},
  {"NdrConformantVaryingStructFree", 1210, 810672u, NULL},
  {"NdrConformantVaryingStructMarshall", 1211, 812032u, NULL},
  {"NdrConformantVaryingStructMemorySize", 1212, 837008u, NULL},
  {"NdrConformantVaryingStructUnmarshall", 1213, 820928u, NULL},
  {"NdrContextHandleInitialize", 1214, 79984u, NULL},
  {"NdrContextHandleSize", 1215, 816416u, NULL},
  {"NdrConvert", 1216, 847520u, NULL},
  {"NdrConvert2", 1217, 847808u, NULL},
  {"NdrCorrelationFree", 1218, 128240u, NULL},
  {"NdrCorrelationInitialize", 1219, 374096u, NULL},
  {"NdrCorrelationPass", 1220, 848448u, NULL},
  {"NdrCreateServerInterfaceFromStub", 1221, 824608u, NULL},
  {"NdrDcomAsyncClientCall", 1222, 824704u, NULL},
  {"NdrDcomAsyncStubCall", 1223, 824832u, NULL},
  {"NdrDllCanUnloadNow", 1224, 370928u, NULL},
  {"NdrDllGetClassObject", 1225, 371008u, NULL},
  {"NdrDllRegisterProxy", 1226, 824960u, NULL},
  {"NdrDllUnregisterProxy", 1227, 825072u, NULL},
  {"NdrEncapsulatedUnionBufferSize", 1228, 422912u, NULL},
  {"NdrEncapsulatedUnionFree", 1229, 391280u, NULL},
  {"NdrEncapsulatedUnionMarshall", 1230, 354224u, NULL},
  {"NdrEncapsulatedUnionMemorySize", 1231, 428720u, NULL},
  {"NdrEncapsulatedUnionUnmarshall", 1232, 427376u, NULL},
  {"NdrFixedArrayBufferSize", 1233, 380000u, NULL},
  {"NdrFixedArrayFree", 1234, 415536u, NULL},
  {"NdrFixedArrayMarshall", 1235, 376032u, NULL},
  {"NdrFixedArrayMemorySize", 1236, 382416u, NULL},
  {"NdrFixedArrayUnmarshall", 1237, 379760u, NULL},
  {"NdrFreeBuffer", 1238, 369472u, NULL},
  {"NdrFullPointerXlatFree", 1239, 128288u, NULL},
  {"NdrFullPointerXlatInit", 1240, 72336u, NULL},
  {"NdrGetBaseInterfaceFromStub", 1241, 825184u, NULL},
  {"NdrGetBuffer", 1242, 323216u, NULL},
  {"NdrGetDcomProtocolVersion", 1243, 825296u, NULL},
  {"NdrGetSimpleTypeBufferAlignment", 1244, 848560u, NULL},
  {"NdrGetSimpleTypeBufferSize", 1245, 848592u, NULL},
  {"NdrGetSimpleTypeMemorySize", 1246, 420448u, NULL},
  {"NdrGetTypeFlags", 1247, 410848u, NULL},
  {"NdrGetUserMarshalInfo", 1248, 363104u, NULL},
  {"NdrInterfacePointerBufferSize", 1249, 816512u, NULL},
  {"NdrInterfacePointerFree", 1250, 810832u, NULL},
  {"NdrInterfacePointerMarshall", 1251, 812384u, NULL},
  {"NdrInterfacePointerMemorySize", 1252, 837504u, NULL},
  {"NdrInterfacePointerUnmarshall", 1253, 495968u, NULL},
  {"NdrMapCommAndFaultStatus", 1254, 406800u, NULL},
  {"NdrMesProcEncodeDecode", 1255, 827984u, NULL},
  {"NdrMesProcEncodeDecode2", 1256, 828976u, NULL},
  {"NdrMesProcEncodeDecode3", 1257, 14176u, NULL},
  {"NdrMesSimpleTypeAlignSize", 1258, 829728u, NULL},
  {"NdrMesSimpleTypeAlignSizeAll", 1259, 887232u, NULL},
  {"NdrMesSimpleTypeDecode", 1260, 829840u, NULL},
  {"NdrMesSimpleTypeDecodeAll", 1261, 887424u, NULL},
  {"NdrMesSimpleTypeEncode", 1262, 830080u, NULL},
  {"NdrMesSimpleTypeEncodeAll", 1263, 887808u, NULL},
  {"NdrMesTypeAlignSize", 1264, 183104u, NULL},
  {"NdrMesTypeAlignSize2", 1265, 144544u, NULL},
  {"NdrMesTypeAlignSize3", 1266, 380480u, NULL},
  {"NdrMesTypeDecode", 1267, 830304u, NULL},
  {"NdrMesTypeDecode2", 1268, 140000u, NULL},
  {"NdrMesTypeDecode3", 1269, 381248u, NULL},
  {"NdrMesTypeEncode", 1270, 830640u, NULL},
  {"NdrMesTypeEncode2", 1271, 145248u, NULL},
  {"NdrMesTypeEncode3", 1272, 380128u, NULL},
  {"NdrMesTypeFree2", 1273, 373584u, NULL},
  {"NdrMesTypeFree3", 1274, 412496u, NULL},
  {"NdrNonConformantStringBufferSize", 1275, 816672u, NULL},
  {"NdrNonConformantStringMarshall", 1276, 812480u, NULL},
  {"NdrNonConformantStringMemorySize", 1277, 428320u, NULL},
  {"NdrNonConformantStringUnmarshall", 1278, 427664u, NULL},
  {"NdrNonEncapsulatedUnionBufferSize", 1279, 183600u, NULL},
  {"NdrNonEncapsulatedUnionFree", 1280, 391344u, NULL},
  {"NdrNonEncapsulatedUnionMarshall", 1281, 354304u, NULL},
  {"NdrNonEncapsulatedUnionMemorySize", 1282, 189232u, NULL},
  {"NdrNonEncapsulatedUnionUnmarshall", 1283, 176800u, NULL},
  {"NdrNsGetBuffer", 1284, 496416u, NULL},
  {"NdrNsSendReceive", 1285, 496416u, NULL},
  {"NdrOleAllocate", 1286, 368240u, NULL},
  {"NdrOleFree", 1287, 368176u, NULL},
  {"NdrOutInit", 1288, 848624u, NULL},
  {"NdrPartialIgnoreClientBufferSize", 1289, 816944u, NULL},
  {"NdrPartialIgnoreClientMarshall", 1290, 812736u, NULL},
  {"NdrPartialIgnoreServerInitialize", 1291, 849008u, NULL},
  {"NdrPartialIgnoreServerUnmarshall", 1292, 822448u, NULL},
  {"NdrPointerBufferSize", 1293, 179952u, NULL},
  {"NdrPointerFree", 1294, 136048u, NULL},
  {"NdrPointerMarshall", 1295, 354912u, NULL},
  {"NdrPointerMemorySize", 1296, 837664u, NULL},
  {"NdrPointerUnmarshall", 1297, 137776u, NULL},
  {"NdrProxyErrorHandler", 1298, 849456u, NULL},
  {"NdrProxyFreeBuffer", 1299, 825392u, NULL},
  {"NdrProxyGetBuffer", 1300, 825472u, NULL},
  {"NdrProxyInitialize", 1301, 825552u, NULL},
  {"NdrProxySendReceive", 1302, 825680u, NULL},
  {"NdrRangeUnmarshall", 1303, 126560u, NULL},
  {"NdrRpcSmClientAllocate", 1304, 851232u, NULL},
  {"NdrRpcSmClientFree", 1305, 851360u, NULL},
  {"NdrRpcSmSetClientToOsf", 1306, 851488u, NULL},
  {"NdrRpcSsDefaultAllocate", 1307, 851536u, NULL},
  {"NdrRpcSsDefaultFree", 1308, 468224u, NULL},
  {"NdrRpcSsDisableAllocate", 1309, 851552u, NULL},
  {"NdrRpcSsEnableAllocate", 1310, 851616u, NULL},
  {"NdrSendReceive", 1311, 496448u, NULL},
  {"NdrServerCall2", 1312, 384352u, NULL},
  {"NdrServerCallAll", 1313, 375872u, NULL},
  {"NdrServerCallNdr64", 1314, 13312u, NULL},
  {"NdrServerContextMarshall", 1315, 812784u, NULL},
  {"NdrServerContextNewMarshall", 1316, 812864u, NULL},
  {"NdrServerContextNewUnmarshall", 1317, 822528u, NULL},
  {"NdrServerContextUnmarshall", 1318, 822864u, NULL},
  {"NdrServerInitialize", 1319, 197504u, NULL},
  {"NdrServerInitializeMarshall", 1320, 440320u, NULL},
  {"NdrServerInitializeNew", 1321, 832688u, NULL},
  {"NdrServerInitializePartial", 1322, 832864u, NULL},
  {"NdrServerInitializeUnmarshall", 1323, 832976u, NULL},
  {"NdrSimpleStructBufferSize", 1324, 159968u, NULL},
  {"NdrSimpleStructFree", 1325, 810880u, NULL},
  {"NdrSimpleStructMarshall", 1326, 160080u, NULL},
  {"NdrSimpleStructMemorySize", 1327, 163024u, NULL},
  {"NdrSimpleStructUnmarshall", 1328, 163200u, NULL},
  {"NdrSimpleTypeMarshall", 1329, 131184u, NULL},
  {"NdrSimpleTypeUnmarshall", 1330, 181216u, NULL},
  {"NdrStubCall2", 1331, 122976u, NULL},
  {"NdrStubCall3", 1332, 119424u, NULL},
  {"NdrStubForwardingFunction", 1333, 413024u, NULL},
  {"NdrStubGetBuffer", 1334, 825760u, NULL},
  {"NdrStubInitialize", 1335, 825856u, NULL},
  {"NdrStubInitializeMarshall", 1336, 825984u, NULL},
  {"NdrTypeFlags", 1337, 995184u, NULL},
  {"NdrTypeFree", 1338, 192896u, NULL},
  {"NdrTypeMarshall", 1339, 131056u, NULL},
  {"NdrTypeSize", 1340, 179824u, NULL},
  {"NdrTypeUnmarshall", 1341, 415456u, NULL},
  {"NdrUnmarshallBasetypeInline", 1342, 376496u, NULL},
  {"NdrUserMarshalBufferSize", 1343, 163488u, NULL},
  {"NdrUserMarshalFree", 1344, 181936u, NULL},
  {"NdrUserMarshalMarshall", 1345, 356240u, NULL},
  {"NdrUserMarshalMemorySize", 1346, 174976u, NULL},
  {"NdrUserMarshalSimpleTypeConvert", 1347, 848016u, NULL},
  {"NdrUserMarshalUnmarshall", 1348, 299856u, NULL},
  {"NdrVaryingArrayBufferSize", 1349, 196944u, NULL},
  {"NdrVaryingArrayFree", 1350, 422224u, NULL},
  {"NdrVaryingArrayMarshall", 1351, 198864u, NULL},
  {"NdrVaryingArrayMemorySize", 1352, 198496u, NULL},
  {"NdrVaryingArrayUnmarshall", 1353, 196288u, NULL},
  {"NdrXmitOrRepAsBufferSize", 1354, 816992u, NULL},
  {"NdrXmitOrRepAsFree", 1355, 810912u, NULL},
  {"NdrXmitOrRepAsMarshall", 1356, 813056u, NULL},
  {"NdrXmitOrRepAsMemorySize", 1357, 838256u, NULL},
  {"NdrXmitOrRepAsUnmarshall", 1358, 822976u, NULL},
  {"NdrpCreateProxy", 1359, 826096u, NULL},
  {"NdrpCreateStub", 1360, 826224u, NULL},
  {"NdrpGetProcFormatString", 1361, 456096u, NULL},
  {"NdrpGetTypeFormatString", 1362, 456256u, NULL},
  {"NdrpGetTypeGenCookie", 1363, 456416u, NULL},
  {"NdrpMemoryIncrement", 1364, 159040u, NULL},
  {"NdrpReleaseTypeFormatString", 1365, 826336u, NULL},
  {"NdrpReleaseTypeGenCookie", 1366, 456352u, NULL},
  {"NdrpVarVtOfTypeDesc", 1367, 456000u, NULL},
  {"RpcAsyncAbortCall", 1368, 10480u, NULL},
  {"RpcAsyncCancelCall", 1369, 424480u, NULL},
  {"RpcAsyncCompleteCall", 1370, 8960u, NULL},
  {"RpcAsyncGetCallStatus", 1371, 199760u, NULL},
  {"RpcAsyncInitializeHandle", 1372, 4464u, NULL},
  {"RpcAsyncRegisterInfo", 1373, 440368u, NULL},
  {"RpcBindingBind", 1374, 322800u, NULL},
  {"RpcBindingCopy", 1375, 389264u, NULL},
  {"RpcBindingCreateA", 1376, 744944u, NULL},
  {"RpcBindingCreateW", 1377, 311536u, NULL},
  {"RpcBindingFree", 1378, 302896u, NULL},
  {"RpcBindingFromStringBindingA", 1379, 419536u, NULL},
  {"RpcBindingFromStringBindingW", 1380, 260288u, NULL},
  {"RpcBindingInqAuthClientA", 1381, 445312u, NULL},
  {"RpcBindingInqAuthClientExA", 1382, 445360u, NULL},
  {"RpcBindingInqAuthClientExW", 1383, 324256u, NULL},
  {"RpcBindingInqAuthClientW", 1384, 324448u, NULL},
  {"RpcBindingInqAuthInfoA", 1385, 745792u, NULL},
  {"RpcBindingInqAuthInfoExA", 1386, 745856u, NULL},
  {"RpcBindingInqAuthInfoExW", 1387, 324688u, NULL},
  {"RpcBindingInqAuthInfoW", 1388, 686144u, NULL},
  {"RpcBindingInqMaxCalls", 1389, 495280u, NULL},
  {"RpcBindingInqObject", 1390, 362432u, NULL},
  {"RpcBindingInqOption", 1391, 716160u, NULL},
  {"RpcBindingReset", 1392, 686272u, NULL},
  {"RpcBindingServerFromClient", 1393, 399472u, NULL},
  {"RpcBindingSetAuthInfoA", 1394, 746032u, NULL},
  {"RpcBindingSetAuthInfoExA", 1395, 746080u, NULL},
  {"RpcBindingSetAuthInfoExW", 1396, 311920u, NULL},
  {"RpcBindingSetAuthInfoW", 1397, 408736u, NULL},
  {"RpcBindingSetObject", 1398, 400336u, NULL},
  {"RpcBindingSetOption", 1399, 59344u, NULL},
  {"RpcBindingToStringBindingA", 1400, 46304u, NULL},
  {"RpcBindingToStringBindingW", 1401, 324576u, NULL},
  {"RpcBindingUnbind", 1402, 404192u, NULL},
  {"RpcBindingVectorFree", 1403, 20848u, NULL},
  {"RpcCancelThread", 1404, 730592u, NULL},
  {"RpcCancelThreadEx", 1405, 730672u, NULL},
  {"RpcCertGeneratePrincipalNameA", 1406, 746320u, NULL},
  {"RpcCertGeneratePrincipalNameW", 1407, 683792u, NULL},
  {"RpcEpRegisterA", 1408, 446336u, NULL},
  {"RpcEpRegisterNoReplaceA", 1409, 747792u, NULL},
  {"RpcEpRegisterNoReplaceW", 1410, 748032u, NULL},
  {"RpcEpRegisterW", 1411, 201984u, NULL},
  {"RpcEpResolveBinding", 1412, 415952u, NULL},
  {"RpcEpUnregister", 1413, 199952u, NULL},
  {"RpcErrorAddRecord", 1414, 743472u, NULL},
  {"RpcErrorClearInformation", 1415, 743792u, NULL},
  {"RpcErrorEndEnumeration", 1416, 422608u, NULL},
  {"RpcErrorGetNextRecord", 1417, 411808u, NULL},
  {"RpcErrorGetNumberOfRecords", 1418, 743808u, NULL},
  {"RpcErrorLoadErrorInfo", 1419, 743856u, NULL},
  {"RpcErrorResetEnumeration", 1420, 743920u, NULL},
  {"RpcErrorSaveErrorInfo", 1421, 743968u, NULL},
  {"RpcErrorStartEnumeration", 1422, 383440u, NULL},
  {"RpcExceptionFilter", 1423, 364368u, NULL},
  {"RpcFreeAuthorizationContext", 1424, 422848u, NULL},
  {"RpcGetAuthorizationContextForClient", 1425, 416080u, NULL},
  {"RpcIfIdVectorFree", 1426, 679120u, NULL},
  {"RpcIfInqId", 1427, 46064u, NULL},
  {"RpcImpersonateClient", 1428, 362656u, NULL},
  {"RpcImpersonateClient2", 1429, 301456u, NULL},
  {"RpcImpersonateClientContainer", 1430, 679232u, NULL},
  {"RpcMgmtEnableIdleCleanup", 1431, 468240u, NULL},
  {"RpcMgmtEpEltInqBegin", 1432, 748192u, NULL},
  {"RpcMgmtEpEltInqDone", 1433, 748880u, NULL},
  {"RpcMgmtEpEltInqNextA", 1434, 749104u, NULL},
  {"RpcMgmtEpEltInqNextW", 1435, 750336u, NULL},
  {"RpcMgmtEpUnregister", 1436, 750448u, NULL},
  {"RpcMgmtInqComTimeout", 1437, 684192u, NULL},
  {"RpcMgmtInqDefaultProtectLevel", 1438, 716288u, NULL},
  {"RpcMgmtInqIfIds", 1439, 679424u, NULL},
  {"RpcMgmtInqServerPrincNameA", 1440, 464400u, NULL},
  {"RpcMgmtInqServerPrincNameW", 1441, 464496u, NULL},
  {"RpcMgmtInqStats", 1442, 679616u, NULL},
  {"RpcMgmtIsServerListening", 1443, 440192u, NULL},
  {"RpcMgmtSetAuthorizationFn", 1444, 750480u, NULL},
  {"RpcMgmtSetCancelTimeout", 1445, 408608u, NULL},
  {"RpcMgmtSetComTimeout", 1446, 59232u, NULL},
  {"RpcMgmtSetServerStackSize", 1447, 418496u, NULL},
  {"RpcMgmtStatsVectorFree", 1448, 679936u, NULL},
  {"RpcMgmtStopServerListening", 1449, 428544u, NULL},
  {"RpcMgmtWaitServerListen", 1450, 680000u, NULL},
  {"RpcNetworkInqProtseqsA", 1451, 746416u, NULL},
  {"RpcNetworkInqProtseqsW", 1452, 429344u, NULL},
  {"RpcNetworkIsProtseqValidA", 1453, 746576u, NULL},
  {"RpcNetworkIsProtseqValidW", 1454, 428928u, NULL},
  {"RpcNsBindingInqEntryNameA", 1455, 746944u, NULL},
  {"RpcNsBindingInqEntryNameW", 1456, 685856u, NULL},
  {"RpcObjectInqType", 1457, 680048u, NULL},
  {"RpcObjectSetInqFn", 1458, 680160u, NULL},
  {"RpcObjectSetType", 1459, 399808u, NULL},
  {"RpcProtseqVectorFreeA", 1460, 747072u, NULL},
  {"RpcProtseqVectorFreeW", 1461, 429216u, NULL},
  {"RpcRaiseException", 1462, 197232u, NULL},
  {"RpcRevertContainerImpersonation", 1463, 680224u, NULL},
  {"RpcRevertToSelf", 1464, 364928u, NULL},
  {"RpcRevertToSelfEx", 1465, 369808u, NULL},
  {"RpcServerCompleteSecurityCallback", 1466, 680320u, NULL},
  {"RpcServerInqBindingHandle", 1467, 310672u, NULL},
  {"RpcServerInqBindings", 1468, 407216u, NULL},
  {"RpcServerInqBindingsEx", 1469, 466672u, NULL},
  {"RpcServerInqCallAttributesA", 1470, 299472u, NULL},
  {"RpcServerInqCallAttributesW", 1471, 300768u, NULL},
  {"RpcServerInqDefaultPrincNameA", 1472, 680416u, NULL},
  {"RpcServerInqDefaultPrincNameW", 1473, 33296u, NULL},
  {"RpcServerInqIf", 1474, 680512u, NULL},
  {"RpcServerInterfaceGroupActivate", 1475, 425824u, NULL},
  {"RpcServerInterfaceGroupClose", 1476, 19920u, NULL},
  {"RpcServerInterfaceGroupCreateA", 1477, 680592u, NULL},
  {"RpcServerInterfaceGroupCreateW", 1478, 435376u, NULL},
  {"RpcServerInterfaceGroupDeactivate", 1479, 19840u, NULL},
  {"RpcServerInterfaceGroupInqBindings", 1480, 681600u, NULL},
  {"RpcServerListen", 1481, 413248u, NULL},
  {"RpcServerRegisterAuthInfoA", 1482, 446000u, NULL},
  {"RpcServerRegisterAuthInfoW", 1483, 34992u, NULL},
  {"RpcServerRegisterIf", 1484, 468016u, NULL},
  {"RpcServerRegisterIf2", 1485, 446176u, NULL},
  {"RpcServerRegisterIf3", 1486, 59680u, NULL},
  {"RpcServerRegisterIfEx", 1487, 419376u, NULL},
  {"RpcServerSubscribeForNotification", 1488, 374432u, NULL},
  {"RpcServerTestCancel", 1489, 322912u, NULL},
  {"RpcServerUnregisterIf", 1490, 55504u, NULL},
  {"RpcServerUnregisterIfEx", 1491, 39840u, NULL},
  {"RpcServerUnsubscribeForNotification", 1492, 374176u, NULL},
  {"RpcServerUseAllProtseqs", 1493, 681680u, NULL},
  {"RpcServerUseAllProtseqsEx", 1494, 681728u, NULL},
  {"RpcServerUseAllProtseqsIf", 1495, 682176u, NULL},
  {"RpcServerUseAllProtseqsIfEx", 1496, 682256u, NULL},
  {"RpcServerUseProtseqA", 1497, 445504u, NULL},
  {"RpcServerUseProtseqEpA", 1498, 747088u, NULL},
  {"RpcServerUseProtseqEpExA", 1499, 747184u, NULL},
  {"RpcServerUseProtseqEpExW", 1500, 467152u, NULL},
  {"RpcServerUseProtseqEpW", 1501, 193136u, NULL},
  {"RpcServerUseProtseqExA", 1502, 747232u, NULL},
  {"RpcServerUseProtseqExW", 1503, 467600u, NULL},
  {"RpcServerUseProtseqIfA", 1504, 747280u, NULL},
  {"RpcServerUseProtseqIfExA", 1505, 747360u, NULL},
  {"RpcServerUseProtseqIfExW", 1506, 464992u, NULL},
  {"RpcServerUseProtseqIfW", 1507, 464912u, NULL},
  {"RpcServerUseProtseqW", 1508, 196784u, NULL},
  {"RpcServerYield", 1509, 730768u, NULL},
  {"RpcSmAllocate", 1510, 851712u, NULL},
  {"RpcSmClientFree", 1511, 851792u, NULL},
  {"RpcSmDestroyClientContext", 1512, 421312u, NULL},
  {"RpcSmDisableAllocate", 1513, 851824u, NULL},
  {"RpcSmEnableAllocate", 1514, 851872u, NULL},
  {"RpcSmFree", 1515, 851920u, NULL},
  {"RpcSmGetThreadHandle", 1516, 851952u, NULL},
  {"RpcSmSetClientAllocFree", 1517, 852016u, NULL},
  {"RpcSmSetThreadHandle", 1518, 852048u, NULL},
  {"RpcSmSwapClientAllocFree", 1519, 852080u, NULL},
  {"RpcSsAllocate", 1520, 852112u, NULL},
  {"RpcSsContextLockExclusive", 1521, 405280u, NULL},
  {"RpcSsContextLockShared", 1522, 807760u, NULL},
  {"RpcSsDestroyClientContext", 1523, 421344u, NULL},
  {"RpcSsDisableAllocate", 1524, 852256u, NULL},
  {"RpcSsDontSerializeContext", 1525, 1093657u, "RPCRT4.I_RpcSsDontSerializeContext"},
  {"RpcSsEnableAllocate", 1526, 852272u, NULL},
  {"RpcSsFree", 1527, 852288u, NULL},
  {"RpcSsGetContextBinding", 1528, 417184u, NULL},
  {"RpcSsGetThreadHandle", 1529, 852304u, NULL},
  {"RpcSsSetClientAllocFree", 1530, 852400u, NULL},
  {"RpcSsSetThreadHandle", 1531, 852512u, NULL},
  {"RpcSsSwapClientAllocFree", 1532, 852720u, NULL},
  {"RpcStringBindingComposeA", 1533, 40032u, NULL},
  {"RpcStringBindingComposeW", 1534, 56672u, NULL},
  {"RpcStringBindingParseA", 1535, 46400u, NULL},
  {"RpcStringBindingParseW", 1536, 250384u, NULL},
  {"RpcStringFreeA", 1537, 426384u, NULL},
  {"RpcStringFreeW", 1538, 59104u, NULL},
  {"RpcTestCancel", 1539, 730848u, NULL},
  {"RpcUserFree", 1540, 833120u, NULL},
  {"SimpleTypeAlignment", 1541, 1000528u, NULL},
  {"SimpleTypeBufferSize", 1542, 1000336u, NULL},
  {"SimpleTypeMemorySize", 1543, 996064u, NULL},
  {"TowerConstruct", 1544, 68192u, NULL},
  {"TowerExplode", 1545, 66784u, NULL},
  {"UuidCompare", 1546, 400096u, NULL},
  {"UuidCreate", 1547, 363312u, NULL},
  {"UuidCreateNil", 1548, 446160u, NULL},
  {"UuidCreateSequential", 1549, 401840u, NULL},
  {"UuidEqual", 1550, 373216u, NULL},
  {"UuidFromStringA", 1551, 444944u, NULL},
  {"UuidFromStringW", 1552, 43408u, NULL},
  {"UuidHash", 1553, 15408u, NULL},
  {"UuidIsNil", 1554, 400496u, NULL},
  {"UuidToStringA", 1555, 427248u, NULL},
  {"UuidToStringW", 1556, 44912u, NULL},
};
#define N 557
#define DLL "rpcrt4.dll"

TEST(exp_rpcrt4_resolve_00) { /* ordinals 1000..1015 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 0, 16);
}
TEST(exp_rpcrt4_resolve_01) { /* ordinals 1016..1031 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 16, 32);
}
TEST(exp_rpcrt4_resolve_02) { /* ordinals 1032..1047 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 32, 48);
}
TEST(exp_rpcrt4_resolve_03) { /* ordinals 1048..1063 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 48, 64);
}
TEST(exp_rpcrt4_resolve_04) { /* ordinals 1064..1079 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 64, 80);
}
TEST(exp_rpcrt4_resolve_05) { /* ordinals 1080..1095 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 80, 96);
}
TEST(exp_rpcrt4_resolve_06) { /* ordinals 1096..1111 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 96, 112);
}
TEST(exp_rpcrt4_resolve_07) { /* ordinals 1112..1127 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 112, 128);
}
TEST(exp_rpcrt4_resolve_08) { /* ordinals 1128..1143 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 128, 144);
}
TEST(exp_rpcrt4_resolve_09) { /* ordinals 1144..1159 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 144, 160);
}
TEST(exp_rpcrt4_resolve_10) { /* ordinals 1160..1175 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 160, 176);
}
TEST(exp_rpcrt4_resolve_11) { /* ordinals 1176..1191 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 176, 192);
}
TEST(exp_rpcrt4_resolve_12) { /* ordinals 1192..1207 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 192, 208);
}
TEST(exp_rpcrt4_resolve_13) { /* ordinals 1208..1223 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 208, 224);
}
TEST(exp_rpcrt4_resolve_14) { /* ordinals 1224..1239 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 224, 240);
}
TEST(exp_rpcrt4_resolve_15) { /* ordinals 1240..1255 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 240, 256);
}
TEST(exp_rpcrt4_resolve_16) { /* ordinals 1256..1271 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 256, 272);
}
TEST(exp_rpcrt4_resolve_17) { /* ordinals 1272..1287 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 272, 288);
}
TEST(exp_rpcrt4_resolve_18) { /* ordinals 1288..1303 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 288, 304);
}
TEST(exp_rpcrt4_resolve_19) { /* ordinals 1304..1319 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 304, 320);
}
TEST(exp_rpcrt4_resolve_20) { /* ordinals 1320..1335 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 320, 336);
}
TEST(exp_rpcrt4_resolve_21) { /* ordinals 1336..1351 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 336, 352);
}
TEST(exp_rpcrt4_resolve_22) { /* ordinals 1352..1367 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 352, 368);
}
TEST(exp_rpcrt4_resolve_23) { /* ordinals 1368..1383 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 368, 384);
}
TEST(exp_rpcrt4_resolve_24) { /* ordinals 1384..1399 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 384, 400);
}
TEST(exp_rpcrt4_resolve_25) { /* ordinals 1400..1415 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 400, 416);
}
TEST(exp_rpcrt4_resolve_26) { /* ordinals 1416..1431 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 416, 432);
}
TEST(exp_rpcrt4_resolve_27) { /* ordinals 1432..1447 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 432, 448);
}
TEST(exp_rpcrt4_resolve_28) { /* ordinals 1448..1463 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 448, 464);
}
TEST(exp_rpcrt4_resolve_29) { /* ordinals 1464..1479 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 464, 480);
}
TEST(exp_rpcrt4_resolve_30) { /* ordinals 1480..1495 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 480, 496);
}
TEST(exp_rpcrt4_resolve_31) { /* ordinals 1496..1511 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 496, 512);
}
TEST(exp_rpcrt4_resolve_32) { /* ordinals 1512..1527 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 512, 528);
}
TEST(exp_rpcrt4_resolve_33) { /* ordinals 1528..1543 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 528, 544);
}
TEST(exp_rpcrt4_resolve_34) { /* ordinals 1544..1556 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 544, 557);
}
TEST(exp_rpcrt4_export_directory) { REQUIRE(t_module(DLL)); exp_check_dir(DLL, tbl, N, 1000, 1556, 556); }
TEST(exp_rpcrt4_alias_groups) { REQUIRE(t_module(DLL)); exp_check_alias_groups(DLL, tbl, N); }
TEST(exp_rpcrt4_gap_ordinals) { REQUIRE(t_module(DLL)); exp_check_gaps(DLL, tbl, N, 1000, 1556); }
TEST(exp_rpcrt4_negative_lookups) { REQUIRE(t_module(DLL)); exp_check_negatives(DLL, "I_RpcBindingInqCurrentModifiedId", 1556); }
TEST(exp_rpcrt4_name_count_and_unique) {
    int names = 0;
    for (int i = 0; i < N; i++) { if (tbl[i].name) names++; for (int j = i + 1; j < N; j++) { CHECK(tbl[i].ord != tbl[j].ord); if (tbl[i].name && tbl[j].name) CHECK(strcmp(tbl[i].name, tbl[j].name)); } }
    EXPECT_EQ(names, 556); EXPECT_EQ(N, 557);
}
TEST(exp_rpcrt4_forwarders) {
    REQUIRE(t_module(DLL));
    { FARPROC a = t_proc_in(DLL, "I_RpcBindingInqDynamicEndpoint"), b = t_proc_in("rpcrt4.dll", "I_RpcBindingInqDynamicEndpointW"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 1049); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "I_RpcNsBindingSetEntryName"), b = t_proc_in("rpcrt4.dll", "I_RpcNsBindingSetEntryNameW"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 1089); CHECK(c == a); }
    { FARPROC a = t_proc_in(DLL, "RpcSsDontSerializeContext"), b = t_proc_in("rpcrt4.dll", "I_RpcSsDontSerializeContext"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 1525); CHECK(c == a); }
}
TEST(exp_rpcrt4_forwarder_target_in_other_image) {
    REQUIRE(t_module(DLL));
    for (int i = 0; i < N; i++) if (tbl[i].fwd) { FARPROC p = t_proc_in(DLL, tbl[i].name); HMODULE h = NULL; if (p) GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h); CHECK(h != NULL); }
}
TEST(exp_rpcrt4_ordinal_only) {
    REQUIRE(t_module(DLL));
    { FARPROC p = t_proc_ord(DLL, 1000); CHECK(p != NULL); CHECK(exp_in_image(t_module(DLL), p)); }
}
TEST(exp_rpcrt4_same_module_forward_resolves_in_image) {
    HMODULE m = t_module(DLL); REQUIRE(m);
    FARPROC p = t_proc_in(DLL, "RpcSsDontSerializeContext");
    CHECK(p != NULL); if (p) CHECK(exp_in_image(m, p));
    CHECK(t_proc_in(DLL, "RpcSsDontSerializeContext") == t_proc_in(DLL, "I_RpcSsDontSerializeContext"));
}

TEST(exp_rpcrt4_name_ordinal_tables) { REQUIRE(t_module(DLL)); exp_check_name_ordinal_tables(DLL); }
TEST(exp_rpcrt4_rva_slots) { REQUIRE(t_module(DLL)); exp_check_rva_slots(DLL, tbl, N); }
TEST(exp_rpcrt4_executable_pages) { REQUIRE(t_module(DLL)); exp_check_exec_pages(DLL, tbl, N); }
TEST(exp_rpcrt4_ldr_agrees) { REQUIRE(t_module(DLL)); exp_check_ldr(DLL, tbl, N, 1556); }
TEST(exp_rpcrt4_table_shape) { exp_check_table_shape(DLL, tbl, N); }
TEST(exp_rpcrt4_handles_and_image) { REQUIRE(t_module(DLL)); exp_check_handles(DLL, "I_RpcBindingInqCurrentModifiedId"); }
