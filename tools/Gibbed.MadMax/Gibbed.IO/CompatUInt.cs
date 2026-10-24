namespace Gibbed.IO
{
    // Compatibility shims: old Gibbed.MadMax code passes uint lengths.
    public static class CompatUIntStreamHelpers
    {
        public static System.IO.MemoryStream ReadToMemoryStream(this System.IO.Stream stream, uint size)
        {
            return stream.ReadToMemoryStream((int)size);
        }

        public static string ReadString(this System.IO.Stream stream, uint size, bool trailingNull, System.Text.Encoding encoding)
        {
            return stream.ReadString((int)size, trailingNull, encoding);
        }
    }
}
