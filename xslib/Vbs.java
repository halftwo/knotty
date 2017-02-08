
import java.io.UnsupportedEncodingException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;

@SuppressWarnings({ "rawtypes", "unchecked" })
public class Vbs
{
	public static final byte VBS_TAIL 	= 0x01;
	public static final byte VBS_LIST 	= 0x02;
	public static final byte VBS_DICT 	= 0x03;
	public static final byte VBS_EXT 	= 0x0E;
	public static final byte VBS_NULL 	= 0x0F;
	public static final byte VBS_BOOL 	= 0x18;
	public static final byte VBS_BLOB 	= 0x1B;
	public static final byte VBS_DECIMAL 	= 0x1C;
	public static final byte VBS_FLOATING 	= 0x1E;
	public static final byte VBS_STRING 	= 0x20;
	public static final byte VBS_INTEGER	= 0x40;

	private static final int FLT_ZERO_ZERO	= 0;	// 0.0 for backward compatibility
	private static final int FLT_ZERO	= 1;	// +0.0
	private static final int FLT_INF	= 2;	// +inf
	private static final int FLT_NAN	= 3;	// +NaN

	public static final Object TAIL_OBJECT = new Object();

	public static class DecodeCtx
	{
		byte[] buf;
		int i;	// start index
		String charsetName;
	};

	/**
	 * the charset only effect the String object
	 * 
	 * @param obj
	 * @param charsetName
	 * @return
	 */
	public static byte[] encode(Object obj, String charsetName)
	{
		try {
			ByteArrayOutputStream out = new ByteArrayOutputStream();
			_pack(out, obj, charsetName);
			return out.toByteArray();
		}
		catch (java.io.IOException ex)
		{
			byte[] vbs = {};
			return vbs;
		}
	}

	/**
	 * the charset only effect the String object
	 * 
	 * @param vbs
	 * @param charsetName
	 * @return
	 */
	public static Object decode(byte[] vbs, String charsetName)
	{
		DecodeCtx ctx = new DecodeCtx();
		ctx.buf = vbs;
		ctx.i = 0;
		ctx.charsetName = charsetName;
		Object obj = _decode(ctx);
		if (obj == TAIL_OBJECT)
			return null;
		return obj;
	}

	public static Object decode(DecodeCtx ctx)
	{
		Object obj = _decode(ctx);
		if (obj == TAIL_OBJECT)
			return null;

		return obj;
	}

	public static void pack(OutputStream out, Object obj, String charsetName) throws java.io.IOException
	{
		_pack(out, obj, charsetName);
	}

	public static void pack(OutputStream out, long n) throws java.io.IOException
	{
		_pack_integer(out, n);
	}

	public static void pack(OutputStream out, double v) throws java.io.IOException
	{
		DoubleMember dm = new DoubleMember();
		_break_double(v, dm);
		if (dm.significant < 0)
		{
			_pack_num_tag(out, (byte)(VBS_FLOATING + 1), -dm.significant);
		}
		else
		{
			_pack_num_tag(out, VBS_FLOATING, dm.significant);
		}
		_pack_integer(out, dm.expo);
	}

	public static void pack(OutputStream out, boolean v) throws java.io.IOException
	{
		out.write((byte)(VBS_BOOL + (v ? 1 : 0)));
	}

	public static void pack(OutputStream out, byte[] v) throws java.io.IOException
	{
		_pack_num_tag(out, VBS_BLOB, v.length);
		out.write(v);
	}

	public static void pack(OutputStream out, String s, String charsetName) throws java.io.IOException
	{
		byte[] x = s.getBytes(charsetName);
		_pack_strlen(out, x.length);
		out.write(x);
	}

	public static void pack(OutputStream out, List list, String charsetName) throws java.io.IOException
	{
		out.write((byte)VBS_LIST);
		Iterator iter = list.iterator();
		while (iter.hasNext())
		{
			Object obj = iter.next();
			_pack(out, obj, charsetName);
		}
		out.write((byte)VBS_TAIL);
	}

	public static void pack(OutputStream out, Map map, String charsetName) throws java.io.IOException
	{
		out.write((byte)VBS_DICT);
		Iterator iter = map.entrySet().iterator();
		while (iter.hasNext())
		{
			Map.Entry entry = (Map.Entry) iter.next();
			_pack(out, entry.getKey(), charsetName);
			_pack(out, entry.getValue(), charsetName);
		}
		out.write((byte)VBS_TAIL);
	}

	public static void pack_null(OutputStream out) throws java.io.IOException
	{
		out.write((byte)VBS_NULL);
	}

	private static void _pack(OutputStream out, Object obj, String charsetName) throws java.io.IOException
	{
		if (obj == null)
		{
			pack_null(out);
		}
		else if (obj instanceof Integer || obj instanceof Long ||
			obj instanceof Short || obj instanceof Byte)
		{
			_pack_integer(out, ((Number)obj).longValue());
		}
		else if (obj instanceof String)
		{
			pack(out, obj.toString(), charsetName);
		}
		else if (obj instanceof Boolean)
		{
			pack(out, ((Boolean)obj).booleanValue());
		}
		else if (obj instanceof byte[])
		{
			pack(out, (byte[])obj);
		}
		else if (obj instanceof Double)
		{
			pack(out, ((Double)obj).doubleValue());
		}
		else if (obj instanceof Float)
		{
			pack(out, ((Float)obj).doubleValue());
		}
		else if (obj instanceof List)
		{
			pack(out, (List)obj, charsetName);
		}
		else if (obj instanceof Map)
		{
			pack(out, (Map)obj, charsetName);
		}
		else
		{
			pack(out, "XXX-not-pack-unsupported-type-" + obj.getClass().getName(), "UTF-8");
		}
	}

	private static void _pack_num_tag(OutputStream out, byte tag, long n) throws java.io.IOException
	{
		assert(n >= 0);
		while (n > 0)
		{
			out.write((byte)(0x80 + (n & 0x7F)));
			n = n >>> 7;
		}
		out.write(tag);
	}

	private static void _pack_integer(OutputStream out, long n) throws java.io.IOException
	{
		byte type = VBS_INTEGER;
		if (n < 0)
		{
			n = -n;
			type |= (byte)0x20;
		}

		while (n >= 0x20)
		{
			out.write((byte)(0x80 + (n & 0x7F)));
			n = n >>> 7;
		}
		out.write((byte)(type | n));
	}

	private static void _pack_strlen(OutputStream out, long n) throws java.io.IOException
	{
		assert(n >= 0);
		while (n >= 0x20)
		{
			out.write((byte)(0x80 + (n & 0x7F)));
			n = n >>> 7;
		}
		out.write((byte)(VBS_STRING | n));
	}

	private static int _find_first_bit_set(long v)
	{
		if (v == 0)
			return 0;

		int r = 1;
		while ((v & 0x1) == 0)
		{
			v >>>= 1;
			++r;
		}

		return r;
	}

	private static int _find_last_bit_set(long v)
	{
		if (v == 0)
			return 0;

		int r = 64;
		while (v > 0)
		{
			v <<= 1;
			--r;
		}

		return r;
	}

	private static class DoubleMember
	{
		public long significant;
		public int expo;
	};

	private static void _break_double(double v, DoubleMember dm)
	{
		long x = Double.doubleToLongBits(v);
		boolean negative = (x & 0x8000000000000000L) != 0L;
		long expo = (x & 0x7ff0000000000000L) >> 52;
		long significant = x & 0x000fffffffffffffL;

		if (expo == 0x7ff)
		{
			expo = (significant != 0) ? FLT_NAN : FLT_INF;
			significant = 0;
		}
		else if (expo == 0)
		{
			if (significant == 0)
				expo = FLT_ZERO_ZERO;
			else
				expo = 1;
		}
		else
		{
			significant |= (1L << 52);
		}
		
		if (significant != 0)
		{
			int shift = _find_first_bit_set(significant) - 1;
			significant >>>= shift;
			expo = expo - 52 + shift - 1023;
		}

		dm.significant = negative ? -significant : significant;
		dm.expo = (int)expo;
	}

	private static double _make_double(long significant, int expo)
	{
		boolean negative = false;

		if (significant != 0)
		{
			int point;
			int shift;

			if (significant < 0)
			{
				significant = -significant;
				negative = true;
			}

			point = _find_last_bit_set(significant) - 1;
			expo = expo + point + 1023;

			if (expo >= 0x7ff)
			{
				return negative ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
			}
			else
			{
				if (expo <= 0)
				{
					shift = 52 - (point + 1) + expo;
					expo = 0;
				}
				else
				{
					shift = 52 - point;
				}

				long bits = (((shift >= 0) ? (significant << shift) : (significant >>> -shift)) & 0x000fffffffffffffL);
				bits |= (((long)expo << 52) & 0x7ff0000000000000L);
				if (negative)
					bits |= 0x8000000000000000L;
				return Double.longBitsToDouble(bits);
			}
		}
		else
		{
			if (expo < 0)
			{
				expo = -expo;
				negative = true;
			}

			if (expo <= FLT_ZERO)
			{
				return negative ? -0.0 : 0.0;
			}
			else if (expo == FLT_INF)
			{
				return negative ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
			}
			else
			{
				return Double.NaN;
			}
		}
	}

	private static class TagInfo
	{
		byte type;
		long num;
	};

	private static int _unpack_tag(DecodeCtx ctx, TagInfo info)
	{
		long num = 0;
		byte type = 0;
		boolean negative = false;
		long x;

		if (ctx.i >= ctx.buf.length)
			return -2;

		x = (long)ctx.buf[ctx.i++];
		if (x >= 0)
		{
			if (x >= VBS_STRING)
			{
				type = (byte)(x & 0x60);
				num = (long)(x & 0x1F);
				if (type == 0x60)
				{
					type = VBS_INTEGER;
					negative = true;
				}
			}
			else if (x >= VBS_BOOL)
			{
				type = (byte)x;
				if (x <= VBS_BOOL + 1)
				{
					num = (x & 0x1);
				}

				if (x != VBS_BLOB)
				{
					type &= ~0x1;
				}

				/* negative has no effect when num == 0.
				 */
			}
			else 
				type = (byte)x;
		}
		else
		{
			int left, shift = 7;

			if (ctx.i >= ctx.buf.length)
				return -2;

			num = (long)(x & 0x7F);
			for (x = ctx.buf[ctx.i++]; x < 0; x = ctx.buf[ctx.i++], shift += 7)
			{
				if (ctx.i >= ctx.buf.length)
					return -2;
				x &= 0x7F;
				left = 64 - shift;
				if (left <= 0 || (left < 7 && x >= (1<<left)))
					return -1;
				num |= ((long)x) << shift;
			}

			if (x >= VBS_STRING)
			{
				type = (byte)(x & 0x60);
				x &= 0x1F;
				if (x != 0)
				{
					left = 64 - shift;
					if (left <= 0 || (left < 7 && x >= (1<<left)))
						return -1;
					num |= ((long)x) << shift;
				}
				if (type == 0x60)
				{
					type = VBS_INTEGER;
					negative = true;
				}
			}
			else if (x >= VBS_BOOL)
			{
				type = (byte)x;
				if (x != VBS_BLOB)
				{
					type &= ~0x1;
					if ((x & 0x1) != 0)
						negative = true;
				}
			}
			else 
				type = (byte)x;
		}

		info.type = type;
		info.num = negative ? -num : num;
		return 0;
	}

	private static int _decode_integer(DecodeCtx ctx, TagInfo info)
	{
		if (_unpack_tag(ctx, info) < 0)
			return -1;
		if (info.type != VBS_INTEGER)
			return -1;
		return 0;
	}

	private static int _decode_list(DecodeCtx ctx, List list)
	{
		while (true)
		{
			Object value = _decode(ctx);
			if (value == TAIL_OBJECT)
				return 0;

			list.add(value);
		}
	}

	private static int _decode_dict(DecodeCtx ctx, Map map)
	{
		while (true)
		{
			Object key = _decode(ctx);
			if (key == TAIL_OBJECT)
				return 0;

			if (key == null)
				return -1;

			Object value = _decode(ctx);
			if (value == TAIL_OBJECT)
				return -1;

			map.put(key, value);
		}
	}
	
	private static Object _decode(DecodeCtx ctx)
	{
		TagInfo info = new TagInfo();
		if (_unpack_tag(ctx, info) < 0)
			return null;

		switch (info.type)
		{
		case VBS_INTEGER:
			return info.num;
		case VBS_STRING:
			{
				if (info.num > ctx.buf.length - ctx.i)
					return null;
				int start = ctx.i;
				ctx.i += info.num;
				try {
					return new String(ctx.buf, start, (int)info.num, ctx.charsetName);
				}
				catch (Exception ex)
				{
					return null;
				}
			}
		case VBS_BLOB:
			{
				if (info.num > ctx.buf.length - ctx.i)
					return null;
				int start = ctx.i;
				ctx.i += info.num;
				return Arrays.copyOfRange(ctx.buf, start, ctx.i);
			}
		case VBS_BOOL:
			return info.num != 0;
		case VBS_FLOATING:
			{
				TagInfo x = new TagInfo();
				if (_decode_integer(ctx, x) < 0)
				{
					return null;
				}
				return _make_double(info.num, (int)x.num);
			}
		case VBS_LIST:
			{
				ArrayList list = new ArrayList();
				if (_decode_list(ctx, list) < 0)
					return null;
				return list;
			}
		case VBS_DICT:
			{
				Map map = new HashMap();
				if (_decode_dict(ctx, map) < 0)
					return null;
				return map;
			}
		case VBS_TAIL:
			return TAIL_OBJECT;
		case VBS_EXT:
			// TODO: what to do?
			if (info.num > ctx.buf.length - ctx.i)
				return null;
			ctx.i += info.num;
			return new Object();
		case VBS_NULL:
			return null;
		}
		return null;
	}

	public static void main(String[] args) throws UnsupportedEncodingException, IOException
	{
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		pack(out, out, "UTF-8");
		pack(out, 1234.5f, "UTF-8");
		System.out.println(decode(out.toByteArray(), "UTF-8"));

		Map<String, Object> m = new HashMap<String, Object>();
		m.put("abc", 32);
		m.put("hello", "You jump, I jump");
		m.put("world", "All men are created equal".getBytes("UTF-8"));
		m.put("float", 12345.6789E123);

		for (int i = 0; i < 1024*1024*1; ++i)
		{
			byte[] tmp = encode(m, "GBK");
		}

		byte[] v = encode(m, "GBK");
		System.out.println(v.length);

		for (int i = 0; i < 1024*1024*1; ++i)
		{
			Object obj = decode(v, "GBK");
		}

		Object obj = decode(v, "GBK");
		System.out.println(obj);
	}
}

