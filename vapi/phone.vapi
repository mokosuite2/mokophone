namespace MokoPhone {
    [CCode (cheader_filename = "callwin.h")]

    [CCode (cname="phone_call_win_activate")]
    public static void call_win_activate();

    [CCode (cheader_filename = "phonewin.h")]

    [CCode (cname = "", cprefix = "", has_type_id = false)]
    public enum PhoneSection {
        SECTION_PHONE,
        SECTION_LOG,
        SECTION_CONTACTS,

        NUM_SECTIONS
    }

    [CCode (cname="phone_win_activate")]
    public static void phone_win_activate(int section, bool callwin);

}
