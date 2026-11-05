# COVERAGE

Status: **verified on genuine Windows 10 22H2** (build 19045, 10.0.19045, x64). The corpus was first written and checked
under Wine only, which left many wrong expectations; every expectation has since been reconciled with real Windows 10
results (the test was fixed, never the check weakened: version dependence is a `t_note`, environment dependence a
`t_skip` with a reason). Wine is no longer the reference; it only explains where some tests came from.

## Export coverage
`tools/coverage_fast.py` compares the public surface (`tools/public_surface.txt`, 1222 names) with names referenced
in non-export-sweep tests: 1159 are exercised by a behavioural test, 63 are not.
Uncovered by design: `Ndr64AsyncClientCall`, `NdrContextHandleInitialize`, `NdrMesProcEncodeDecode2`,
`NdrProxyForwardingFunction3..32` (vtable thunks, only reachable through a proxy), `ObjectStublessClient5..31`
(same), `STGMEDIUM_UserUnmarshal64`, `VARIANT_UserFree64/Marshal64/Unmarshal64` (covered only by presence in the
`exports_*` areas). Non-public named exports are checked by `exports_*` and `gap_10` for presence/forwarding.

## Measured Windows 10 results
Final run, three fresh guests per pass, per-exe limit 600 s. Modes are described in `docs/WIN10_NOTES.md`.

| pass | exes | tests | failed | skipped | checks passed | checks failed |
|---|---|---|---|---|---|---|
| `adm` (elevated administrator, 50 non-dx areas) | 50 | 7924 | 0 | 4 | 1,003,834 | 0 |
| `std` (restricted-admin token, same areas) | 50 | 7924 | 0 | 18 | 1,003,452 | 0 |
| `dx` (destructive areas, elevated) | 16 | 580 | 0 | 2 | 5,782 | 0 |

Total 66 exes, 8504 distinct tests (`rpc_ext_internal`, 33 tests, was added afterwards: Windows 10 22H2 `adm` and `std`, 0 failed, 0 skipped; it is not in the totals above). The three `both` guests gave identical per-test results (the totals above are one guest's), see Determinism.

### Determinism (three guests)
- `adm`/`std`: zero test-result differences between the three guests; only the `NOTE` line of `picture_font_ole` that prints the
  `SelectPicture(NULL dc)` bitmap handle value differs (value-volatile; it shows only when whole lines are compared, `win10_compare.py --lines`).
- `dx`: guests 1 and 2 identical. Guest 3 identical except `dx_sechost_service_lifecycle`, which hit the 600 s timeout inside
  `dx_svc_delete_removes_from_enum` (an SCM enumeration right after `DeleteService`) while the other two guests passed all 153
  tests. It looks like a guest SCM wedge, not an expectation problem, but it has not been root-caused: treat that test as
  timing-sensitive. So the run as a whole is **not** strictly deterministic.

### Per-area results (Windows 10, `adm` pass; `dx_*` from the `dx` pass)
Every area: 0 failed tests, 0 failed checks. Runtime is the wall time of the exe on the first guest.

| area | tests | failed | skipped adm | skipped std | checks (adm pass) | s |
|---|---|---|---|---|---|---|
| bstr_core | 113 | 0 | 0 | 0 | 11825 | 2.0 |
| com_class_factories | 132 | 0 | 0 | 0 | 1404 | 1.6 |
| com_context_threading | 171 | 0 | 0 | 0 | 1612 | 6.7 |
| com_init_apartments | 179 | 0 | 0 | 0 | 4196 | 1.1 |
| com_marshal_basic | 186 | 0 | 0 | 0 | 1693 | 2.9 |
| com_marshal_xproc | 126 | 0 | 0 | 0 | 1265 | 7.7 |
| com_memory_guid_strings | 155 | 0 | 0 | 0 | 10920 | 1.2 |
| core_smoke | 3 | 0 | 0 | 0 | 4 | 0.2 |
| dispatch_typeinfo_dispcall | 292 | 0 | 0 | 0 | 2178 | 0.7 |
| dx_com_class_factories | 4 | 0 | 0 | n/a (dx) | 18 | 0.5 |
| dx_com_registry_activation | 151 | 0 | 0 | n/a (dx) | 504 | 35.7 |
| dx_gap_00 | 5 | 0 | 0 | n/a (dx) | 47 | 0.2 |
| dx_gap_01 | 24 | 0 | 0 | n/a (dx) | 190 | 0.7 |
| dx_gap_02 | 9 | 0 | 0 | n/a (dx) | 40 | 0.6 |
| dx_gap_03 | 8 | 0 | 0 | n/a (dx) | 36 | 0.5 |
| dx_gap_04 | 12 | 0 | 0 | n/a (dx) | 35 | 2.9 |
| dx_gap_06 | 5 | 0 | 0 | n/a (dx) | 42 | 0.3 |
| dx_gap_07 | 3 | 0 | 2 | n/a (dx) | 5 | 1.0 |
| dx_gap_08 | 3 | 0 | 0 | n/a (dx) | 16 | 8.4 |
| dx_gap_09 | 3 | 0 | 0 | n/a (dx) | 72 | 0.1 |
| dx_gap_10 | 4 | 0 | 0 | n/a (dx) | 4 | 0.2 |
| dx_ole2_clipboard | 15 | 0 | 0 | n/a (dx) | 120 | 0.5 |
| dx_rpc_ep_register | 9 | 0 | 0 | n/a (dx) | 58 | 1.0 |
| dx_sechost_service_lifecycle | 153 | 0 | 0 | n/a (dx) | 745 | 9.7 |
| dx_typelib_marshal_registry | 172 | 0 | 0 | n/a (dx) | 3850 | 10.0 |
| exports_combase | 243 | 0 | 0 | 0 | 27698 | 1.7 |
| exports_ole32 | 101 | 0 | 0 | 0 | 8673 | 0.3 |
| exports_oleaut32_rpcrt4_sechost | 120 | 0 | 0 | 0 | 584839 | 0.7 |
| gap_00 | 60 | 0 | 0 | 0 | 3611 | 1.2 |
| gap_01 | 72 | 0 | 0 | 1 | 4255 | 1.0 |
| gap_02 | 82 | 0 | 0 | 0 | 37146 | 0.3 |
| gap_03 | 108 | 0 | 0 | 0 | 1248 | 0.7 |
| gap_04 | 120 | 0 | 0 | 0 | 899 | 5.8 |
| gap_05 | 70 | 0 | 0 | 0 | 2852 | 1.6 |
| gap_06 | 148 | 0 | 0 | 0 | 13591 | 1.2 |
| gap_07 | 93 | 0 | 0 | 0 | 6883 | 0.7 |
| gap_08 | 80 | 0 | 0 | 1 | 21588 | 1.4 |
| gap_09 | 106 | 0 | 0 | 0 | 5154 | 0.6 |
| gap_10 | 96 | 0 | 0 | 0 | 2355 | 0.3 |
| monikers_rot | 223 | 0 | 0 | 0 | 2543 | 6.2 |
| ole2_core_helpers | 264 | 0 | 0 | 0 | 2066 | 17.5 |
| picture_font_ole | 259 | 0 | 0 | 0 | 2419 | 1.4 |
| propvariant_propstg | 235 | 0 | 0 | 0 | 3971 | 8.9 |
| rpc_ep_mgmt_misc | 124 | 0 | 0 | 0 | 806 | 1.3 |
| rpc_ndr_marshal_formats | 219 | 0 | 0 | 0 | 3941 | 0.3 |
| rpc_proxy_stub_dcom | 131 | 0 | 0 | 0 | 1675 | 0.6 |
| rpc_server_client_ncalrpc | 201 | 0 | 0 | 0 | 1107 | 22.4 |
| rpc_ext_internal | 33 | 0 | 0 | 0 | 336 | 2 |
| rpc_uuid_strings | 133 | 0 | 0 | 0 | 14103 | 1.0 |
| safearray_core | 137 | 0 | 0 | 0 | 4504 | 0.7 |
| sechost_etw_trace | 162 | 0 | 0 | 1 | 715 | 1.2 |
| sechost_security_sid_sd | 248 | 0 | 0 | 0 | 5249 | 0.5 |
| sechost_service_ctrl_local | 132 | 0 | 2 | 2 | 568 | 24.2 |
| sechost_services_query | 337 | 0 | 2 | 13 | 1365 | 4.7 |
| storage_create_open | 167 | 0 | 0 | 0 | 1419 | 7.3 |
| storage_elements_streams | 458 | 0 | 0 | 0 | 2474 | 2.8 |
| storage_lockbytes_misc | 168 | 0 | 0 | 0 | 1032 | 0.8 |
| typelib_create_roundtrip | 135 | 0 | 0 | 0 | 5832 | 3.8 |
| typelib_load_info | 150 | 0 | 0 | 0 | 5126 | 1.2 |
| usrmarshal_ole32 | 91 | 0 | 0 | 0 | 43065 | 0.5 |
| usrmarshal_oleaut32 | 107 | 0 | 0 | 0 | 20426 | 0.4 |
| variant_arith_ops | 93 | 0 | 0 | 0 | 52864 | 1.8 |
| variant_convert_numeric | 234 | 0 | 0 | 0 | 50191 | 0.7 |
| variant_core | 188 | 0 | 0 | 0 | 2410 | 0.5 |
| variant_format_parse | 176 | 0 | 0 | 0 | 6248 | 1.2 |
| winrt_hstring | 161 | 0 | 0 | 0 | 10552 | 1.7 |
| winrt_roapi | 135 | 0 | 0 | 0 | 1274 | 1.1 |

## Remaining skips and known differences
Skips (all environment or token dependent; the skip text is the reason printed by the test):
- `adm` (4): `sechost_service_ctrl_local` `scl_sd_set_null_descriptor` (Windows faults on a NULL security descriptor, not exercised) and
  `scl_openservice_stop_right_denied_for_users` (needs a non-elevated token); `sechost_services_query` `svcdyn_not_a_service_process` and
  `svcdyn_null_handle` (`QueryServiceDynamicInformation` faults with an access violation outside a service process).
- `std` (18): the same four minus `scl_openservice_stop_right_denied_for_users`, which runs, plus: `gap_01`
  `gap1_event_access_query_size_protocol` (needs read access to the trace security key); `gap_08`
  `gap8_startservicea_running_service_already_running` (no start right on RpcSs); `sechost_etw_trace` `etw_qat_too_small_array_more_data`
  (fewer than two visible trace sessions); `sechost_service_ctrl_local` `scl_lock_lifecycle` (SC_MANAGER_LOCK needs admin);
  `sechost_services_query` eleven `svcx_*`/`svcdep_*` dependent-service tests (handle rights not granted to the token).
- `dx` (2): `dx_gap_07` `dx_gap7_ep_inq_next_a_empty_annotation_registration` and
  `dx_gap7_ep_inq_next_a_returns_ansi_annotation_object_and_binding` ("cannot start ncalrpc server" on the guest).

Known differences and caveats:
- `std` is a restricted copy of the administrator token (Medium integrity), not a real standard account; `TokenIsElevated` may still read 1.
- Locale and SKU: the reference guest is the Home SKU, en-US; `NOTE` lines print `?` for non-ASCII text (the runner masks
  non-printable bytes on the serial line). Version-dependent facts are recorded as `NOTE`, not checked.
- 63 public names are uncovered by design (see Export coverage).
- Wine origin: many tests are rewritten Wine tests; the tests that carried `NOWINE()`/`t_is_wine()` guards are no-ops on Windows.
  The old Wine failure counts were wrong expectations or Wine bugs and are no longer tracked here.
