/**
 * Implementazione di un servizio di notifiche per mokopanel
 */

[DBus (name = "org.mokosuite.Phone")]
public class MokophoneService : Object
{
    public MokophoneService()
    {
        try {
            var conn = DBus.Bus.get (DBus.BusType.SESSION);
            /*
             * NAME ALREADY OWNED BY REMOTE CONFIGURATION SERVICE
            dynamic DBus.Object bus = conn.get_object ("org.freedesktop.DBus",
                                                    "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus");

            // try to register service in session bus
            uint reply = bus.request_name ("org.mokosuite.phone", (uint) 0);
            assert (reply == DBus.RequestNameReply.PRIMARY_OWNER);
            */

            conn.register_object ("/org/mokosuite/Phone", this);
        }

        catch (DBus.Error e) {
            error ("%s", e.message);
        }
    }

    public void Frontend(string section)
    {
        // il telefono e' la sezione predefinita
        int n_section = MokoPhone.PhoneSection.SECTION_PHONE;

        if (section == "log") n_section = MokoPhone.PhoneSection.SECTION_LOG;
        else if (section == "contacts") n_section = MokoPhone.PhoneSection.SECTION_CONTACTS;
        else if (section == "calls") {
            MokoPhone.call_win_activate();
            return;
        }

        // non e' piu' necessario attivare la callwin, ora abbiamo il panel con le chiamate in corso ;)
        MokoPhone.phone_win_activate(n_section, false);
    }
}

