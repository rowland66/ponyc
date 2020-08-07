actor FileStream is OutStream
  """
  Asynchronous access to a File object. Wraps file operations print, write,
  printv and writev. The File will be disposed through File._final.
  """
  let _file: File
  let _encoder: StringEncoder

  new create(file: File iso, encoder: StringEncoder = UTF8StringEncoder) =>
    _file = consume file
    _encoder = encoder

  be print(data: (String | ByteSeq)) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _file.write(data, _encoder)
    _file.write("\n", _encoder)

  be write(data: (String | ByteSeq)) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _file.write(data, _encoder)

  be printv(data: (StringIter | ByteSeqIter)) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    _file.printv(data, _encoder)

  be writev(data: (StringIter | ByteSeqIter)) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    _file.writev(data, _encoder)

  be flush() =>
    """
    Flush pending data to write.
    """
    _file.flush()
