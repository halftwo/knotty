
import java.net.Socket;
import java.io.PrintStream;
import java.io.OutputStream;
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.Date;
import java.text.SimpleDateFormat;

public class Dlog
{
	/* Always print to stderr in addition to write to dlogd. */
	public static final int DLOG_STDERR = 0x01;
	/* If failed to connect dlogd, print to stderr. */
	public static final int DLOG_PERROR = 0x02;

	private static String _charset = "UTF8";
	private static int _option = 0;
	private static String _identity = "-";
	private static short _pid = 0;
	private static OutputStream _out = null;

	private static final String DLOGD_IP = "127.0.0.1";
	private static final int DLOGD_PORT = 6109;

	private static final int RECORD_VERSION = 4;
	private static final int TYPE_RAW = 0;

	private static final int IDENT_MAX = 63;
	private static final int TAG_MAX = 63;
	private static final int LOCUS_MAX = 127;
	private static final int RECORD_MAX_SIZE = 4000;
	private static final int RECORD_HEAD_SIZE = 16;

	public static void dlog_set(String ident, int option)
	{
		if (ident.isEmpty())
			_identity = "-";
		else if (ident.length() > IDENT_MAX)
			_identity = ident.substring(0, IDENT_MAX);
		else
			_identity = ident;
		_option = option;
		_pid = (short)_getpid();
	}

	public static void setCharset(String charset)
	{
		_charset = charset;
	}

	public static void dlog(String tag, String format, Object... args)
	{
		xdlog("", tag, "", format, args);
	}

	public static void xdlog(String identity, String tag, String locus, String format, Object... args)
	{
		try {
			boolean perror_done = false;
			boolean failed = false;
			if (identity == "")
				identity = _identity;

			int i_len = identity.length();
			if (i_len > IDENT_MAX)
			{
				i_len = IDENT_MAX;
				identity = identity.substring(0, IDENT_MAX);
			}
			else if (i_len == 0)
			{
				i_len = 1;
				identity = "-";
			}

			int t_len = tag.length();
			if (t_len > TAG_MAX)
			{
				t_len = TAG_MAX;
				tag = tag.substring(0, TAG_MAX);
			}
			else if (t_len == 0)
			{
				t_len = 1;
				tag = "-";
			}

			int l_len = locus.length();
			if (l_len > LOCUS_MAX)
			{
				l_len = LOCUS_MAX;
				locus = locus.substring(0, LOCUS_MAX);
			}
			else if (l_len == 0)
			{
				l_len = 1;
				locus = "-";
			}

			StringBuilder sb = new StringBuilder(256);
			sb.append(identity);
			sb.append(' ');
			sb.append(tag);
			sb.append(' ');
			sb.append(locus);
			sb.append(' ');
			sb.append(String.format(format, args));

			String str = sb.toString();
			int len = str.length();
			while (str.charAt(--len) == '\n')
				continue;
			++len;
			if (len != str.length()) {
				str = str.substring(0, len);
			}

			byte[] bs = str.getBytes(_charset);
			len = RECORD_HEAD_SIZE + bs.length;

			byte[] rec = new byte[RECORD_MAX_SIZE];
			int truncated = 0x00;
			if (len > rec.length - 1)
			{
				len = rec.length - 1;
				truncated = 0x80;
			}
			System.arraycopy(bs, 0, rec, RECORD_HEAD_SIZE, len - RECORD_HEAD_SIZE);
			rec[len++] = 0;

			String timestr = null;
			if ((_option & DLOG_STDERR) != 0) {
				timestr = _gen_timestr();
				perror_done = true;
				System.err.printf("%s %s\n", timestr, str);
			}

			rec[0] = (byte)((len>>8)&0xff);
			rec[1] = (byte)(len & 0xff);
			/* truncated:1, type:3, bigendian:1, version:3 */
			rec[2] = (byte)(truncated | (TYPE_RAW << 4) | 0x08 | RECORD_VERSION);
			rec[3] = (byte)(i_len + t_len + l_len + 2);
			rec[4] = (byte)0;
			rec[5] = (byte)0;
			rec[6] = (byte)((_pid>>>8)&0xff);
			rec[7] = (byte)(_pid & 0xff);


			if (_out == null) {
				_out = _connect_daemon();
			}

			if (_out != null) {
				try {
					_out.write(rec, 0, len);
					_out.flush();
				} catch (IOException e) {
					failed = true;
					_out.close();
					_out = null;
				} 
			}
			else
				failed = true;

			if (failed) {
				if ((_option & DLOG_PERROR) != 0 && !perror_done) {
					if (timestr == null)
						timestr = _gen_timestr();
					System.err.printf("%s %s\n", timestr, str);
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public static void sdlog(String identity, String tag, String locus, String str)
	{
		xdlog(identity, tag, locus, "%s", str);
	}

	private static String _gen_timestr()
	{
		String timestr = new SimpleDateFormat("yyMMdd-HHmmss").format(new Date());
		if (_pid != 0)
			timestr += " " + _pid;
		return timestr;
	}

	private static int _getpid()
	{
		return 0;
	}

	private static OutputStream _connect_daemon()
	{
		try {
			Socket socket = new Socket(DLOGD_IP, DLOGD_PORT);
			_out = socket.getOutputStream();
			return _out;
		} catch (Exception e) {
			/* do nothing. */
			return null;
		}
	}

	public static void main(String[] args)
	{
		Dlog.dlog_set("Dlog.java", Dlog.DLOG_PERROR);
		Dlog.dlog("TEST", "haha %d", 1);
	}
}

