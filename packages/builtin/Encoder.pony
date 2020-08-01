trait val Encoder
  """
  An Encoder converts unicode codepoints into a variable number of bytes.
  """

  fun encode(value: U32): (USize, U8, U8, U8, U8)
  """
  Convert a codepoint into up to 4 bytes. The first value in the returned tuple indicates the number of
  bytes required for the encoding. The next 4 values contain the encoded bytes.
  """
