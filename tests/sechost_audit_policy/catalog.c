/* The audit catalog: category and subcategory GUIDs, counts, names and the argument rules of the enumeration and
 * lookup exports. None of it needs a privilege (adm and std give the same answers on Windows 10). */
#include "sap.h"

TEST(sap_categories_exact_set) {
    NEEDAP(AuditEnumerateCategories); NEEDAP(AuditFree);
    GUID *c = NULL; ULONG n = 0;
    SET_ERR(0x5a5a);
    CHECK(pAuditEnumerateCategories(&c, &n));
    EXPECT_EQ(n, 9);
    if (!c) return;
    for (int i = 0; i < 9; i++) CHECKF(sap_has_guid(c, n, &SAP_CAT[i]), "category %d (%s) missing", i, SAP_CAT_NAME[i]);
    EXPECT_EQ(LocalSize(c), n * sizeof(GUID));
    GUID *c2 = NULL; ULONG n2 = 0;
    CHECK(pAuditEnumerateCategories(&c2, &n2));
    CHECK(c2 != c);
    if (c2 && n2 == n) CHECK(!memcmp(c, c2, n * sizeof(GUID)));    /* a stable order */
    EXPECT_EQ(pAuditFree(c), NULL);
    if (c2) pAuditFree(c2);
}

TEST(sap_category_names) {
    NEEDAP(AuditLookupCategoryNameW); NEEDAP(AuditFree);
    for (int i = 0; i < 9; i++) {
        PWSTR nm = NULL;
        SET_ERR(0x5a5a);
        CHECKF(pAuditLookupCategoryNameW(&SAP_CAT[i], &nm), "category %d lookup failed %lu", i, GetLastError());
        if (!nm) continue;
        char a[128]; sap_narrow(nm, a, sizeof a);
        if (sap_english()) EXPECT_STR(a, SAP_CAT_NAME[i]); else t_note("category %d name \"%s\" (locale dependent)", i, a);
        EXPECT_EQ(LocalSize(nm), (lstrlenW(nm) + 1) * sizeof(WCHAR));
        pAuditFree(nm);
    }
}

TEST(sap_subcategory_catalog_all) {
    NEEDAP(AuditEnumerateSubCategories); NEEDAP(AuditFree);
    GUID *all = NULL; ULONG n = 0;
    CHECK(pAuditEnumerateSubCategories(NULL, TRUE, &all, &n));
    EXPECT_EQ(n, 59);
    if (!all || n != 59) return;
    EXPECT_EQ(LocalSize(all), n * sizeof(GUID));
    for (int i = 0; i < 59; i++) { GUID g = sap_sub_guid(i); CHECKF(sap_has_guid(all, n, &g), "subcategory %d (%s) missing", i, SAP_SUB[i].name); }
    /* the full listing is ordered by GUID value (not grouped by category) */
    int ascending = 1;
    for (ULONG i = 1; i < n; i++) if (all[i - 1].Data1 >= all[i].Data1) ascending = 0;
    CHECK(ascending);
    EXPECT_EQ(all[0].Data1, 0x0CCE9210); EXPECT_EQ(all[58].Data1, 0x0CCE924A);
    pAuditFree(all);
}

TEST(sap_subcategories_per_category) {
    NEEDAP(AuditEnumerateSubCategories); NEEDAP(AuditFree);
    ULONG total = 0;
    for (int c = 0; c < 9; c++) {
        GUID *s = NULL; ULONG n = 0;
        SET_ERR(0x5a5a);
        CHECKF(pAuditEnumerateSubCategories(&SAP_CAT[c], FALSE, &s, &n), "category %d: %lu", c, GetLastError());
        EXPECT_EQ(n, SAP_CAT_COUNT[c]);
        total += n;
        if (!s) continue;
        for (ULONG k = 1; k < n; k++) CHECK(s[k - 1].Data1 < s[k].Data1);
        for (int i = 0; i < 59; i++) {
            GUID g = sap_sub_guid(i);
            CHECKF(sap_has_guid(s, n, &g) == (SAP_SUB[i].cat == c), "subcategory %d (%s) vs category %d", i, SAP_SUB[i].name, c);
        }
        pAuditFree(s);
    }
    EXPECT_EQ(total, 59);
    /* `all` wins over the category, whatever the category GUID is */
    GUID *a = NULL; ULONG n = 0;
    CHECK(pAuditEnumerateSubCategories(&SAP_CAT[CAT_LOGON], TRUE, &a, &n)); EXPECT_EQ(n, 59); if (a) pAuditFree(a);
    a = NULL; n = 0;
    CHECK(pAuditEnumerateSubCategories(&SAP_BOGUS, TRUE, &a, &n)); EXPECT_EQ(n, 59); if (a) pAuditFree(a);
}

TEST(sap_subcategory_enum_errors) {
    NEEDAP(AuditEnumerateSubCategories);
    GUID *p = (GUID *)(ULONG_PTR)1; ULONG n = 77;
    SET_ERR(0x5a5a); CHECK(!pAuditEnumerateSubCategories(NULL, FALSE, &p, &n)); EXPECT_ERR(87);
    p = (GUID *)(ULONG_PTR)1; n = 77;
    SET_ERR(0x5a5a); CHECK(!pAuditEnumerateSubCategories(&SAP_BOGUS, FALSE, &p, &n)); EXPECT_ERR(87);
    EXPECT_EQ(p, 1); EXPECT_EQ(n, 77);                               /* the outputs are left alone */
    GUID sub = sap_sub_guid(SUB_LOGON);
    SET_ERR(0x5a5a); CHECK(!pAuditEnumerateSubCategories(&sub, FALSE, &p, &n)); EXPECT_ERR(87);   /* a subcategory is no category */
    SET_ERR(0x5a5a); CHECK(!pAuditEnumerateSubCategories(&SAP_CAT[0], FALSE, NULL, &n)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditEnumerateSubCategories(&SAP_CAT[0], FALSE, &p, NULL)); EXPECT_ERR(87);
}

TEST(sap_subcategory_names_exact) {
    NEEDAP(AuditLookupSubCategoryNameW); NEEDAP(AuditFree);
    for (int i = 0; i < 59; i++) {
        GUID g = sap_sub_guid(i); PWSTR nm = NULL;
        SET_ERR(0x5a5a);
        CHECKF(pAuditLookupSubCategoryNameW(&g, &nm), "subcategory %d lookup failed %lu", i, GetLastError());
        if (!nm) continue;
        char a[128]; sap_narrow(nm, a, sizeof a);
        if (sap_english()) EXPECT_STR(a, SAP_SUB[i].name); else CHECK(a[0] != 0);
        EXPECT_EQ(LocalSize(nm), (lstrlenW(nm) + 1) * sizeof(WCHAR));
        pAuditFree(nm);
    }
}

TEST(sap_lookup_errors) {
    NEEDAP(AuditLookupCategoryNameW); NEEDAP(AuditLookupSubCategoryNameW);
    GUID sub = sap_sub_guid(SUB_LOGON);
    PWSTR nm = (PWSTR)(ULONG_PTR)1;
    SET_ERR(0x5a5a); CHECK(!pAuditLookupCategoryNameW(&SAP_BOGUS, &nm)); EXPECT_ERR(87); EXPECT_EQ(nm, 1);
    SET_ERR(0x5a5a); CHECK(!pAuditLookupCategoryNameW(&sub, &nm)); EXPECT_ERR(87);          /* a subcategory GUID is no category */
    SET_ERR(0x5a5a); CHECK(!pAuditLookupSubCategoryNameW(&SAP_BOGUS, &nm)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditLookupSubCategoryNameW(&SAP_CAT[CAT_LOGON], &nm)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditLookupCategoryNameW(NULL, &nm)); EXPECT_ERR(E_NULLREF);
    SET_ERR(0x5a5a); CHECK(!pAuditLookupSubCategoryNameW(NULL, &nm)); EXPECT_ERR(E_NULLREF);
    EXPECT_EQ(nm, 1);
}
