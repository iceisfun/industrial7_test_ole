#include "exp.h"

static const exp_ent tbl[] = {
  {"I_ScSetServiceBitsA", 1, 285456u, NULL},
  {"I_ScSetServiceBitsW", 2, 116896u, NULL},
  {"AuditComputeEffectivePolicyBySid", 3, 114240u, NULL},
  {"AuditEnumerateCategories", 4, 327488u, NULL},
  {"AuditEnumeratePerUserPolicy", 5, 327824u, NULL},
  {"AuditEnumerateSubCategories", 6, 327968u, NULL},
  {"AuditFree", 7, 59920u, NULL},
  {"AuditLookupCategoryNameW", 8, 328336u, NULL},
  {"AuditLookupSubCategoryNameW", 9, 328720u, NULL},
  {"AuditQueryGlobalSaclW", 10, 329104u, NULL},
  {"AuditQueryPerUserPolicy", 11, 107296u, NULL},
  {"AuditQuerySecurity", 12, 329216u, NULL},
  {"AuditQuerySystemPolicy", 13, 107632u, NULL},
  {"AuditSetGlobalSaclW", 14, 329424u, NULL},
  {"AuditSetPerUserPolicy", 15, 329536u, NULL},
  {"AuditSetSecurity", 16, 329712u, NULL},
  {"AuditSetSystemPolicy", 17, 330064u, NULL},
  {"BuildSecurityDescriptorForSharingAccess", 18, 104256u, NULL},
  {"BuildSecurityDescriptorForSharingAccessEx", 19, 102576u, NULL},
  {"CapabilityCheck", 20, 55056u, NULL},
  {"CapabilityCheckForSingleSessionSku", 21, 283632u, NULL},
  {"ChangeServiceConfig2A", 22, 285888u, NULL},
  {"ChangeServiceConfig2W", 23, 116752u, NULL},
  {"ChangeServiceConfigA", 24, 286320u, NULL},
  {"ChangeServiceConfigW", 25, 58928u, NULL},
  {"CloseServiceHandle", 26, 34016u, NULL},
  {"CloseTrace", 27, 47808u, NULL},
  {"ControlService", 28, 59712u, NULL},
  {"ControlServiceExA", 29, 287056u, NULL},
  {"ControlServiceExW", 30, 58224u, NULL},
  {"ControlTraceA", 31, 309136u, NULL},
  {"ControlTraceW", 32, 37632u, NULL},
  {"ConvertSDToStringSDRootDomainW", 33, 183136u, NULL},
  {"ConvertSecurityDescriptorToStringSecurityDescriptorW", 34, 69648u, NULL},
  {"ConvertSidToStringSidW", 35, 61632u, NULL},
  {"ConvertStringSDToSDDomainA", 36, 183264u, NULL},
  {"ConvertStringSDToSDDomainW", 37, 183584u, NULL},
  {"ConvertStringSDToSDRootDomainW", 38, 183792u, NULL},
  {"ConvertStringSecurityDescriptorToSecurityDescriptorW", 39, 69808u, NULL},
  {"ConvertStringSidToSidW", 40, 72416u, NULL},
  {"CreateIsolatedProcess", 41, 398464u, NULL},
  {"CreateIsolationContainer", 42, 398608u, NULL},
  {"CreateServiceA", 43, 287408u, NULL},
  {"CreateServiceEx", 44, 289296u, NULL},
  {"CreateServiceW", 45, 290576u, NULL},
  {"CredBackupCredentials", 46, 330832u, NULL},
  {"CredDeleteA", 47, 331248u, NULL},
  {"CredDeleteW", 48, 331488u, NULL},
  {"CredEncryptAndMarshalBinaryBlob", 49, 336528u, NULL},
  {"CredEnumerateA", 50, 331728u, NULL},
  {"CredEnumerateW", 51, 61952u, NULL},
  {"CredFindBestCredentialA", 52, 332080u, NULL},
  {"CredFindBestCredentialW", 53, 332400u, NULL},
  {"CredFree", 54, 59920u, NULL},
  {"CredGetSessionTypes", 55, 332720u, NULL},
  {"CredGetTargetInfoA", 56, 332864u, NULL},
  {"CredGetTargetInfoW", 57, 333184u, NULL},
  {"CredIsMarshaledCredentialW", 58, 336576u, NULL},
  {"CredIsProtectedA", 59, 336640u, NULL},
  {"CredIsProtectedW", 60, 104816u, NULL},
  {"CredMarshalCredentialA", 61, 336800u, NULL},
  {"CredMarshalCredentialW", 62, 110336u, NULL},
  {"CredParseUserNameWithType", 63, 104544u, NULL},
  {"CredProfileLoaded", 64, 333504u, NULL},
  {"CredProfileLoadedEx", 65, 61536u, NULL},
  {"CredProfileUnloaded", 66, 106832u, NULL},
  {"CredProtectA", 67, 336912u, NULL},
  {"CredProtectEx", 68, 109680u, NULL},
  {"CredProtectW", 69, 109648u, NULL},
  {"CredReadA", 70, 333632u, NULL},
  {"CredReadByTokenHandle", 71, 333952u, NULL},
  {"CredReadDomainCredentialsA", 72, 334288u, NULL},
  {"CredReadDomainCredentialsW", 73, 334640u, NULL},
  {"CredReadW", 74, 335008u, NULL},
  {"CredRestoreCredentials", 75, 335328u, NULL},
  {"CredUnmarshalCredentialA", 76, 337296u, NULL},
  {"CredUnmarshalCredentialW", 77, 105040u, NULL},
  {"CredUnprotectA", 78, 337472u, NULL},
  {"CredUnprotectEx", 79, 112064u, NULL},
  {"CredUnprotectW", 80, 337888u, NULL},
  {"CredWriteA", 81, 335712u, NULL},
  {"CredWriteDomainCredentialsA", 82, 335936u, NULL},
  {"CredWriteDomainCredentialsW", 83, 336240u, NULL},
  {"CredWriteW", 84, 60848u, NULL},
  {"CredpConvertCredential", 85, 61024u, NULL},
  {"CredpConvertOneCredentialSize", 86, 68224u, NULL},
  {"CredpConvertTargetInfo", 87, 337920u, NULL},
  {"CredpDecodeCredential", 88, 338528u, NULL},
  {"CredpEncodeCredential", 89, 338608u, NULL},
  {"CredpEncodeSecret", 90, 338736u, NULL},
  {"DeleteIsolationContainer", 91, 398704u, NULL},
  {"DeleteService", 92, 291792u, NULL},
  {"EnableTraceEx2", 93, 39312u, NULL},
  {"EnumDependentServicesW", 94, 106992u, NULL},
  {"EnumServicesStatusExW", 95, 32400u, NULL},
  {"EnumerateIdentityProviders", 96, 51920u, NULL},
  {"EnumerateTraceGuidsEx", 97, 59424u, NULL},
  {"EtwQueryRealtimeConsumer", 98, 306912u, NULL},
  {"EventAccessControl", 99, 310784u, NULL},
  {"EventAccessQuery", 100, 310864u, NULL},
  {"EventAccessRemove", 101, 311456u, NULL},
  {"FreeContainer", 102, 341600u, NULL},
  {"FreeTransientObjectSecurityDescriptor", 103, 99184u, NULL},
  {"GetDefaultIdentityProvider", 104, 113856u, NULL},
  {"GetEmbeddedContainerIsolationPolicy", 105, 341680u, NULL},
  {"GetEmbeddedImageMitigationPolicy", 106, 55440u, NULL},
  {"GetIdentityProviderInfoByGUID", 107, 113456u, NULL},
  {"GetIdentityProviderInfoByName", 108, 172400u, NULL},
  {"GetServiceDirectory", 109, 57904u, NULL},
  {"GetServiceDisplayNameW", 110, 105744u, NULL},
  {"GetServiceKeyNameW", 111, 105936u, NULL},
  {"GetServiceProcessToken", 112, 291920u, NULL},
  {"GetServiceRegistryStateKey", 113, 59312u, NULL},
  {"I_QueryTagInformation", 114, 29984u, NULL},
  {"I_RegisterSvchostNotificationCallback", 115, 59680u, NULL},
  {"I_ScBroadcastServiceControlMessage", 116, 113664u, NULL},
  {"I_ScIsSecurityProcess", 117, 117088u, NULL},
  {"I_ScPnPGetServiceName", 118, 44032u, NULL},
  {"I_ScQueryServiceConfig", 119, 29696u, NULL},
  {"I_ScRegisterDeviceNotification", 120, 47840u, NULL},
  {"I_ScRegisterPreshutdownRestart", 121, 292064u, NULL},
  {"I_ScReparseServiceDatabase", 122, 292272u, NULL},
  {"I_ScRpcBindA", 123, 299184u, NULL},
  {"I_ScRpcBindW", 124, 114128u, NULL},
  {"I_ScSendPnPMessage", 125, 30608u, NULL},
  {"I_ScSendTSMessage", 126, 113664u, NULL},
  {"I_ScUnregisterDeviceNotification", 127, 58752u, NULL},
  {"I_ScValidatePnPService", 128, 44112u, NULL},
  {"LocalGetConditionForString", 129, 66400u, NULL},
  {"LocalGetReferencedTokenTypesForCondition", 130, 190096u, NULL},
  {"LocalGetStringForCondition", 131, 193808u, NULL},
  {"LocalRpcBindingCreateWithSecurity", 132, 284160u, NULL},
  {"LocalRpcBindingSetAuthInfoEx", 133, 284592u, NULL},
  {"LookupAccountNameLocalA", 134, 172656u, NULL},
  {"LookupAccountNameLocalW", 135, 85776u, NULL},
  {"LookupAccountSidLocalA", 136, 173040u, NULL},
  {"LookupAccountSidLocalW", 137, 86944u, NULL},
  {"LsaAddAccountRights", 138, 320784u, NULL},
  {"LsaClose", 139, 64608u, NULL},
  {"LsaCreateSecret", 140, 323728u, NULL},
  {"LsaDelete", 141, 321344u, NULL},
  {"LsaEnumerateAccountRights", 142, 107440u, NULL},
  {"LsaEnumerateAccountsWithUserRight", 143, 320944u, NULL},
  {"LsaFreeMemory", 144, 55408u, NULL},
  {"LsaICLookupNames", 145, 62384u, NULL},
  {"LsaICLookupNamesWithCreds", 146, 321504u, NULL},
  {"LsaICLookupSids", 147, 64032u, NULL},
  {"LsaICLookupSidsWithCreds", 148, 322032u, NULL},
  {"LsaLookupClose", 149, 88240u, NULL},
  {"LsaLookupFreeMemory", 150, 55408u, NULL},
  {"LsaLookupGetDomainInfo", 151, 85616u, NULL},
  {"LsaLookupManageSidNameMapping", 152, 57632u, NULL},
  {"LsaLookupNames2", 153, 62272u, NULL},
  {"LsaLookupOpenLocalPolicy", 154, 88352u, NULL},
  {"LsaLookupSids", 155, 63600u, NULL},
  {"LsaLookupSids2", 156, 322624u, NULL},
  {"LsaLookupTranslateNames", 157, 114688u, NULL},
  {"LsaLookupTranslateSids", 158, 55616u, NULL},
  {"LsaLookupUserAccountType", 159, 55968u, NULL},
  {"LsaOpenPolicy", 160, 59952u, NULL},
  {"LsaOpenSecret", 161, 324000u, NULL},
  {"LsaQueryInformationPolicy", 162, 63280u, NULL},
  {"LsaQuerySecret", 163, 324272u, NULL},
  {"LsaRemoveAccountRights", 164, 321168u, NULL},
  {"LsaRetrievePrivateData", 165, 107760u, NULL},
  {"LsaSetInformationPolicy", 166, 322640u, NULL},
  {"LsaSetSecret", 167, 325456u, NULL},
  {"LsaStorePrivateData", 168, 326048u, NULL},
  {"NotifyServiceStatusChange", 169, 27376u, NULL},
  {"NotifyServiceStatusChangeA", 170, 106800u, NULL},
  {"NotifyServiceStatusChangeW", 171, 27376u, NULL},
  {"OpenSCManagerA", 172, 33328u, NULL},
  {"OpenSCManagerW", 173, 33760u, NULL},
  {"OpenServiceA", 174, 106592u, NULL},
  {"OpenServiceW", 175, 33632u, NULL},
  {"OpenTraceW", 176, 45200u, NULL},
  {"ProcessTrace", 177, 46944u, NULL},
  {"QueryAllTracesA", 178, 312000u, NULL},
  {"QueryAllTracesW", 179, 5104u, NULL},
  {"QueryLocalUserServiceName", 180, 292464u, NULL},
  {"QueryServiceConfig2A", 181, 293360u, NULL},
  {"QueryServiceConfig2W", 182, 30928u, NULL},
  {"QueryServiceConfigA", 183, 294496u, NULL},
  {"QueryServiceConfigW", 184, 33040u, NULL},
  {"QueryServiceDynamicInformation", 185, 296080u, NULL},
  {"QueryServiceObjectSecurity", 186, 294896u, NULL},
  {"QueryServiceStatus", 187, 31664u, NULL},
  {"QueryServiceStatusEx", 188, 33472u, NULL},
  {"QueryTraceProcessingHandle", 189, 307040u, NULL},
  {"QueryTransientObjectSecurityDescriptor", 190, 98768u, NULL},
  {"QueryUserServiceName", 191, 32016u, NULL},
  {"QueryUserServiceNameForContext", 192, 295120u, NULL},
  {"RegisterServiceCtrlHandlerA", 193, 296240u, NULL},
  {"RegisterServiceCtrlHandlerExA", 194, 106272u, NULL},
  {"RegisterServiceCtrlHandlerExW", 195, 21872u, NULL},
  {"RegisterServiceCtrlHandlerW", 196, 59888u, NULL},
  {"RegisterTraceGuidsA", 197, 567337u, "NTDLL.EtwRegisterTraceGuidsA"},
  {"ReleaseIdentityProviderEnumContext", 198, 54480u, NULL},
  {"RemoveTraceCallback", 199, 307584u, NULL},
  {"RpcClientCapabilityCheck", 200, 54832u, NULL},
  {"SetLocalRpcServerInterfaceSecurity", 201, 284864u, NULL},
  {"SetLocalRpcServerProtseqSecurity", 202, 285088u, NULL},
  {"SetServiceObjectSecurity", 203, 116480u, NULL},
  {"SetServiceStatus", 204, 31760u, NULL},
  {"SetTraceCallback", 205, 307824u, NULL},
  {"StartServiceA", 206, 106704u, NULL},
  {"StartServiceCtrlDispatcherA", 207, 296352u, NULL},
  {"StartServiceCtrlDispatcherW", 208, 23264u, NULL},
  {"StartServiceW", 209, 21760u, NULL},
  {"StartTraceA", 210, 312016u, NULL},
  {"StartTraceW", 211, 42032u, NULL},
  {"StopTraceW", 212, 114096u, NULL},
  {"SubscribeServiceChangeNotifications", 213, 57376u, NULL},
  {"TraceQueryInformation", 214, 313344u, NULL},
  {"TraceSetInformation", 215, 314288u, NULL},
  {"UnsubscribeServiceChangeNotifications", 216, 59824u, NULL},
  {"WaitServiceState", 217, 56496u, NULL},
};
#define N 217
#define DLL "sechost.dll"

TEST(exp_sechost_resolve_00) { /* ordinals 1..16 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 0, 16);
}
TEST(exp_sechost_resolve_01) { /* ordinals 17..32 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 16, 32);
}
TEST(exp_sechost_resolve_02) { /* ordinals 33..48 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 32, 48);
}
TEST(exp_sechost_resolve_03) { /* ordinals 49..64 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 48, 64);
}
TEST(exp_sechost_resolve_04) { /* ordinals 65..80 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 64, 80);
}
TEST(exp_sechost_resolve_05) { /* ordinals 81..96 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 80, 96);
}
TEST(exp_sechost_resolve_06) { /* ordinals 97..112 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 96, 112);
}
TEST(exp_sechost_resolve_07) { /* ordinals 113..128 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 112, 128);
}
TEST(exp_sechost_resolve_08) { /* ordinals 129..144 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 128, 144);
}
TEST(exp_sechost_resolve_09) { /* ordinals 145..160 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 144, 160);
}
TEST(exp_sechost_resolve_10) { /* ordinals 161..176 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 160, 176);
}
TEST(exp_sechost_resolve_11) { /* ordinals 177..192 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 176, 192);
}
TEST(exp_sechost_resolve_12) { /* ordinals 193..208 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 192, 208);
}
TEST(exp_sechost_resolve_13) { /* ordinals 209..217 */
    REQUIRE(t_module(DLL));
    exp_check_range(DLL, tbl, 208, 217);
}
TEST(exp_sechost_export_directory) { REQUIRE(t_module(DLL)); exp_check_dir(DLL, tbl, N, 1, 217, 217); }
TEST(exp_sechost_alias_groups) { REQUIRE(t_module(DLL)); exp_check_alias_groups(DLL, tbl, N); }
TEST(exp_sechost_gap_ordinals) { REQUIRE(t_module(DLL)); exp_check_gaps(DLL, tbl, N, 1, 217); }
TEST(exp_sechost_negative_lookups) { REQUIRE(t_module(DLL)); exp_check_negatives(DLL, "I_ScSetServiceBitsA", 217); }
TEST(exp_sechost_name_count_and_unique) {
    int names = 0;
    for (int i = 0; i < N; i++) { if (tbl[i].name) names++; for (int j = i + 1; j < N; j++) { CHECK(tbl[i].ord != tbl[j].ord); if (tbl[i].name && tbl[j].name) CHECK(strcmp(tbl[i].name, tbl[j].name)); } }
    EXPECT_EQ(names, 217); EXPECT_EQ(N, 217);
}
TEST(exp_sechost_forwarders) {
    REQUIRE(t_module(DLL));
    { FARPROC a = t_proc_in(DLL, "RegisterTraceGuidsA"), b = t_proc_in("ntdll.dll", "EtwRegisterTraceGuidsA"); CHECK(a != NULL); CHECK(a == b); FARPROC c = t_proc_ord(DLL, 197); CHECK(c == a); }
}
TEST(exp_sechost_forwarder_target_in_other_image) {
    REQUIRE(t_module(DLL));
    for (int i = 0; i < N; i++) if (tbl[i].fwd) { FARPROC p = t_proc_in(DLL, tbl[i].name); HMODULE h = NULL; if (p) GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h); CHECK(h != NULL); }
}

TEST(exp_sechost_name_ordinal_tables) { REQUIRE(t_module(DLL)); exp_check_name_ordinal_tables(DLL); }
TEST(exp_sechost_rva_slots) { REQUIRE(t_module(DLL)); exp_check_rva_slots(DLL, tbl, N); }
TEST(exp_sechost_executable_pages) { REQUIRE(t_module(DLL)); exp_check_exec_pages(DLL, tbl, N); }
TEST(exp_sechost_ldr_agrees) { REQUIRE(t_module(DLL)); exp_check_ldr(DLL, tbl, N, 217); }
TEST(exp_sechost_table_shape) { exp_check_table_shape(DLL, tbl, N); }
TEST(exp_sechost_handles_and_image) { REQUIRE(t_module(DLL)); exp_check_handles(DLL, "I_ScSetServiceBitsA"); }
