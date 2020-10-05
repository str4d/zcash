use libc::c_char;

use fluent_templates::static_loader;

static_loader! {
    static LOCALES = {
        locales: "./locales",
        fallback_language: "en-US",
        core_locales: "./locales/core.ftl",
    };
}

#[no_mangle]
pub extern "C" fn fluent_tr(
    text_id: *const c_char,
    field_values: *const *const c_char,
    fields_len: usize,
) {
    let text_id = unsafe { CStr::from_ptr(text_id) }.to_str().unwrap();
}
