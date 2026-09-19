using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

namespace ReaderManager
{
    /// <summary>
    /// WinUSB asynchronous communication channel.
    ///
    /// EP4 OUT : PC -> Device
    /// EP5 IN  : Device -> PC
    ///
    /// RX is permanently handled by a background thread.
    ///
    /// All received data is delivered through DataReceived.
    /// Read() drains the internal RX queue (kept for legacy callers such as
    /// OtaUpdater / WinUsbEchoTest); it is non-blocking.
    /// </summary>
    public class WinUsbChannel : IDisposable
    {
        // ============================================================
        // SetupAPI
        // ============================================================

        [StructLayout(LayoutKind.Sequential)]
        private struct SP_DEVICE_INTERFACE_DATA
        {
            public int cbSize;
            public Guid InterfaceClassGuid;
            public int Flags;
            public IntPtr Reserved;
        }

        private const int DIGCF_PRESENT = 0x02;
        private const int DIGCF_DEVICEINTERFACE = 0x10;

        [DllImport(
            "setupapi.dll",
            CharSet = CharSet.Unicode,
            SetLastError = true)]
        private static extern IntPtr SetupDiGetClassDevs(
            ref Guid classGuid,
            string enumerator,
            IntPtr hwndParent,
            int flags);

        [DllImport(
            "setupapi.dll",
            CharSet = CharSet.Unicode,
            SetLastError = true)]
        private static extern bool SetupDiEnumDeviceInterfaces(
            IntPtr devInfoSet,
            IntPtr devInfo,
            ref Guid classGuid,
            int index,
            ref SP_DEVICE_INTERFACE_DATA data);

        [DllImport(
            "setupapi.dll",
            CharSet = CharSet.Unicode,
            SetLastError = true)]
        private static extern bool SetupDiGetDeviceInterfaceDetail(
            IntPtr devInfoSet,
            ref SP_DEVICE_INTERFACE_DATA data,
            IntPtr detailData,
            int detailSize,
            out int requiredSize,
            IntPtr devInfoData);

        [DllImport("setupapi.dll")]
        private static extern bool SetupDiDestroyDeviceInfoList(
            IntPtr devInfoSet);

        private static readonly Guid UsbDevGuid =
            new Guid("A5DCBF10-6530-11D2-901F-00C04FB951ED");


        // ============================================================
        // WinUSB
        // ============================================================

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_Initialize(
            IntPtr deviceHandle,
            out IntPtr interfaceHandle);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_Free(
            IntPtr interfaceHandle);


        // ============================================================
        // ReadPipe
        // ============================================================

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_ReadPipe(
            IntPtr interfaceHandle,
            byte pipeID,
            IntPtr buffer,
            uint bufferLength,
            out uint lengthTransferred,
            IntPtr overlapped);


        // ============================================================
        // WritePipe
        // ============================================================

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_WritePipe(
            IntPtr interfaceHandle,
            byte pipeID,
            byte[] buffer,
            uint bufferLength,
            out uint lengthTransferred,
            IntPtr overlapped);


        // ============================================================
        // Pipe Policy
        // ============================================================

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_SetPipePolicy(
            IntPtr interfaceHandle,
            byte pipeID,
            uint policyType,
            uint valueLength,
            ref byte value);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_SetPipePolicy(
            IntPtr interfaceHandle,
            byte pipeID,
            uint policyType,
            uint valueLength,
            ref uint value);


        // ============================================================
        // Overlapped result
        // ============================================================

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_GetOverlappedResult(
            IntPtr interfaceHandle,
            IntPtr overlapped,
            out uint bytesTransferred,
            bool wait);


        // ============================================================
        // QueryPipe
        // ============================================================

        [StructLayout(LayoutKind.Sequential)]
        private struct WINUSB_PIPE_INFORMATION
        {
            public uint PipeType;
            public ushort PipeId;
            public ushort MaximumPacketSize;
            public uint Interval;
        }

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_QueryPipe(
            IntPtr interfaceHandle,
            byte alternateInterfaceNumber,
            byte pipeIndex,
            out WINUSB_PIPE_INFORMATION pipeInformation);


        // ============================================================
        // USB Pipe Type
        // ============================================================

        private const uint UsbdPipeTypeControl = 0;
        private const uint UsbdPipeTypeIsochronous = 1;
        private const uint UsbdPipeTypeBulk = 2;
        private const uint UsbdPipeTypeInterrupt = 3;


        // ============================================================
        // Pipe Policy constants
        // ============================================================

        /*
         * IMPORTANT:
         *
         * RAW_IO = 0x07
         *
         * 之前使用 0x00 是错误的。
         */

        private const uint RAW_IO = 0x07;

        private const uint PIPE_TRANSFER_TIMEOUT = 0x03;


        // ============================================================
        // OVERLAPPED
        // ============================================================

        [StructLayout(LayoutKind.Sequential)]
        private struct OVERLAPPED
        {
            public IntPtr Internal;
            public IntPtr InternalHigh;
            public uint Offset;
            public uint OffsetHigh;
            public IntPtr hEvent;
        }


        // ============================================================
        // Kernel32
        // ============================================================

        [DllImport(
            "kernel32.dll",
            SetLastError = true)]
        private static extern IntPtr CreateEvent(
            IntPtr lpEventAttributes,
            bool bManualReset,
            bool bInitialState,
            string lpName);

        [DllImport(
            "kernel32.dll",
            SetLastError = true)]
        private static extern bool ResetEvent(
            IntPtr hEvent);

        [DllImport(
            "kernel32.dll",
            SetLastError = true)]
        private static extern uint WaitForSingleObject(
            IntPtr hHandle,
            uint dwMilliseconds);

        [DllImport(
            "kernel32.dll",
            SetLastError = true)]
        private static extern bool CancelIoEx(
            IntPtr hFile,
            IntPtr lpOverlapped);

        [DllImport(
            "kernel32.dll",
            SetLastError = true)]
        private static extern bool CloseHandle(
            IntPtr handle);


        // ============================================================
        // CreateFile
        // ============================================================

        [DllImport(
            "kernel32.dll",
            CharSet = CharSet.Auto,
            SetLastError = true)]
        private static extern IntPtr CreateFile(
            string fileName,
            uint desiredAccess,
            uint shareMode,
            IntPtr securityAttrs,
            uint creationDisposition,
            uint flagsAndAttributes,
            IntPtr templateFile);


        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;

        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;

        private const uint OPEN_EXISTING = 3;

        private const uint FILE_FLAG_OVERLAPPED = 0x40000000;


        private static readonly IntPtr INVALID_HANDLE_VALUE =
            new IntPtr(-1);


        // ============================================================
        // Win32 errors
        // ============================================================

        private const int ERROR_IO_PENDING = 997;
        private const int ERROR_OPERATION_ABORTED = 995;


        // ============================================================
        // Wait
        // ============================================================

        private const uint WAIT_OBJECT_0 = 0x00000000;
        private const uint INFINITE = 0xFFFFFFFF;


        // ============================================================
        // Endpoint
        // ============================================================

        public const byte EP_OUT = 0x04;
        public const byte EP_IN = 0x85;


        // ============================================================
        // RX Buffer
        // ============================================================

        /*
         * EP5 MaxPacketSize 已经确认是 64。
         *
         * 对于 Bulk endpoint：
         *
         *     64 bytes
         *
         * 足够覆盖一个 USB packet。
         *
         * 如果设备一次发送超过 64 bytes，
         * WinUSB 会继续处理后续 packet。
         */

        private const int READ_BUFFER_SIZE = 64;

        /* v9.82d: true = 打印全量调测诊断（PIPE/RAW_IO/逐 ACK 等）；false = 简洁输出。
         * static readonly（非 const）——避免 if(false) 产生 CS0162 不可达代码警告 */
        private static readonly bool _verboseLog = false;


        // ============================================================
        // Handles
        // ============================================================

        private IntPtr _devHandle = IntPtr.Zero;

        private IntPtr _ifHandle = IntPtr.Zero;


        // ============================================================
        // RX resources
        // ============================================================

        private IntPtr _rdBuffer = IntPtr.Zero;

        private IntPtr _rdOvPtr = IntPtr.Zero;

        private IntPtr _rdEvent = IntPtr.Zero;


        // ============================================================
        // RX queue (legacy Read() drain)
        // ============================================================

        private readonly object _rxLock = new object();

        private readonly Queue<byte> _rxQueue = new Queue<byte>();


        // ============================================================
        // RX Thread
        // ============================================================

        private Thread _receiveThread = null;

        private volatile bool _receiveRunning = false;


        // ============================================================
        // State
        // ============================================================

        private volatile bool _opened = false;


        // ============================================================
        // Statistics
        // ============================================================

        private long _rxSubmitCount = 0;

        private long _rxPendingCount = 0;

        private long _rxCompleteCount = 0;

        private long _rxErrorCount = 0;

        private long _rxBytes = 0;

        private int _rxErrorStreak = 0;   /* 连续错误计数（设备断开后抑制刷屏） */


        // ============================================================
        // Stopwatch
        // ============================================================

        private static readonly Stopwatch _sw =
            Stopwatch.StartNew();


        // ============================================================
        // Events
        // ============================================================

        /// <summary>
        /// Status / diagnostic log.
        /// </summary>
        public event Action<string> StatusChanged;


        /// <summary>
        /// USB RX data.
        ///
        /// data:
        ///     Actual received bytes.
        ///
        /// length:
        ///     Actual received length.
        /// </summary>
        public event Action<byte[], int> DataReceived;


        // ============================================================
        // Properties
        // ============================================================

        public bool IsOpen
        {
            get
            {
                return _opened;
            }
        }


        // ============================================================
        // Status
        // ============================================================

        private void Status(string message)
        {
            /* v9.82e: 时间戳统一由上层 Log() 加（此处只传原始消息，防双重前缀） */
            try
            {
                StatusChanged?.Invoke(message);
            }
            catch
            {
                // 日志不能影响 USB 通信
            }
        }


        // ============================================================
        // Open
        // ============================================================

        public bool Open(
            ushort vid,
            ushort pid)
        {
            Close();

            try
            {
                // ----------------------------------------------------
                // Find USB device
                // ----------------------------------------------------

                Guid guid = UsbDevGuid;

                IntPtr devInfoSet =
                    SetupDiGetClassDevs(
                        ref guid,
                        null,
                        IntPtr.Zero,
                        DIGCF_PRESENT |
                        DIGCF_DEVICEINTERFACE);


                if (
                    devInfoSet == IntPtr.Zero ||
                    devInfoSet == INVALID_HANDLE_VALUE)
                {
                    Status(
                        "SetupDiGetClassDevs failed, err=" +
                        Marshal.GetLastWin32Error());

                    return false;
                }


                string wanted =
                    string.Format(
                        "VID_{0:X4}&PID_{1:X4}",
                        vid,
                        pid);


                string devicePath = null;

                int index = 0;


                while (true)
                {
                    SP_DEVICE_INTERFACE_DATA ifData =
                        new SP_DEVICE_INTERFACE_DATA();

                    ifData.cbSize =
                        Marshal.SizeOf(
                            typeof(SP_DEVICE_INTERFACE_DATA));


                    bool enumResult =
                        SetupDiEnumDeviceInterfaces(
                            devInfoSet,
                            IntPtr.Zero,
                            ref guid,
                            index,
                            ref ifData);


                    if (!enumResult)
                    {
                        break;
                    }


                    int requiredSize = 0;


                    SetupDiGetDeviceInterfaceDetail(
                        devInfoSet,
                        ref ifData,
                        IntPtr.Zero,
                        0,
                        out requiredSize,
                        IntPtr.Zero);


                    if (requiredSize <= 0)
                    {
                        index++;
                        continue;
                    }


                    IntPtr detail =
                        Marshal.AllocHGlobal(
                            requiredSize);


                    try
                    {
                        /*
                         * SP_DEVICE_INTERFACE_DETAIL_DATA.cbSize
                         *
                         * x86 = 6
                         * x64 = 8
                         */

                        Marshal.WriteInt32(
                            detail,
                            IntPtr.Size == 8 ? 8 : 6);


                        if (
                            SetupDiGetDeviceInterfaceDetail(
                                devInfoSet,
                                ref ifData,
                                detail,
                                requiredSize,
                                out requiredSize,
                                IntPtr.Zero))
                        {
                            IntPtr pathPtr =
                                IntPtr.Add(
                                    detail,
                                    4);


                            string path =
                                Marshal.PtrToStringUni(
                                    pathPtr);


                            if (
                                path != null &&
                                path.IndexOf(
                                    wanted,
                                    StringComparison.OrdinalIgnoreCase) >= 0)
                            {
                                devicePath = path;
                                break;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(
                            detail);
                    }


                    index++;
                }


                SetupDiDestroyDeviceInfoList(
                    devInfoSet);


                if (string.IsNullOrEmpty(devicePath))
                {
                    Status(
                        string.Format(
                            "WinUSB: device VID_{0:X4}/PID_{1:X4} not found",
                            vid,
                            pid));

                    return false;
                }


                if (_verboseLog)
                {
                    Status(
                        "WinUSB: found " +
                        devicePath);
                }


                // ----------------------------------------------------
                // CreateFile
                // ----------------------------------------------------

                _devHandle =
                    CreateFile(
                        devicePath,
                        GENERIC_READ |
                        GENERIC_WRITE,
                        FILE_SHARE_READ |
                        FILE_SHARE_WRITE,
                        IntPtr.Zero,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        IntPtr.Zero);


                if (
                    _devHandle == INVALID_HANDLE_VALUE ||
                    _devHandle == IntPtr.Zero)
                {
                    int err =
                        Marshal.GetLastWin32Error();


                    Status(
                        "WinUSB: CreateFile failed, err=" +
                        err);


                    _devHandle =
                        IntPtr.Zero;


                    return false;
                }


                if (_verboseLog)
                {
                    Status(
                        "WinUSB: CreateFile OVERLAPPED ok");
                }


                // ----------------------------------------------------
                // Initialize WinUSB
                // ----------------------------------------------------

                if (
                    !WinUsb_Initialize(
                        _devHandle,
                        out _ifHandle))
                {
                    int err =
                        Marshal.GetLastWin32Error();


                    Status(
                        "WinUSB: Initialize failed, err=" +
                        err);


                    CloseHandle(
                        _devHandle);


                    _devHandle =
                        IntPtr.Zero;


                    return false;
                }


                if (_verboseLog)
                {
                    Status(
                        "WinUSB: initialized");
                }


                // ----------------------------------------------------
                // Query endpoint
                // ----------------------------------------------------

                QueryPipes();


                // ----------------------------------------------------
                // RAW_IO
                // ----------------------------------------------------

                SetRawIo(
                    EP_IN,
                    "IN");


                SetRawIo(
                    EP_OUT,
                    "OUT");


                // ----------------------------------------------------
                // Transfer timeout = 0 (infinite) - no URB cancel/re-arm churn
                // ----------------------------------------------------

                uint tmo = 0;

                bool tmoIn =
                    WinUsb_SetPipePolicy(
                        _ifHandle,
                        EP_IN,
                        PIPE_TRANSFER_TIMEOUT,
                        4,
                        ref tmo);

                bool tmoOut =
                    WinUsb_SetPipePolicy(
                        _ifHandle,
                        EP_OUT,
                        PIPE_TRANSFER_TIMEOUT,
                        4,
                        ref tmo);

                if (_verboseLog)
                {
                    Status(
                        string.Format(
                            "WinUSB: PIPE_TRANSFER_TIMEOUT=0 in={0} out={1} err={2}",
                            tmoIn,
                            tmoOut,
                            Marshal.GetLastWin32Error()));
                }


                // ----------------------------------------------------
                // RX resources
                // ----------------------------------------------------

                if (!CreateReceiveResources())
                {
                    Close();

                    return false;
                }


                _opened = true;


                // ----------------------------------------------------
                // Start RX thread
                // ----------------------------------------------------

                _receiveRunning = true;


                _receiveThread =
                    new Thread(
                        ReceiveThreadProc);


                _receiveThread.IsBackground =
                    true;


                _receiveThread.Name =
                    "WinUSB-RX";


                _receiveThread.Start();


                Status(
                    "WinUSB connected");


                return true;
            }
            catch (Exception ex)
            {
                Status(
                    "WinUSB Open exception: " +
                    ex.Message);

                Close();

                return false;
            }
        }


        // ============================================================
        // Query Pipes
        // ============================================================

        private void QueryPipes()
        {
            if (_ifHandle == IntPtr.Zero)
                return;


            if (!_verboseLog)
                return;
            Status(
                "========== USB PIPE INFORMATION ==========");


            for (byte i = 0; i < 16; i++)
            {
                WINUSB_PIPE_INFORMATION info;


                bool result =
                    WinUsb_QueryPipe(
                        _ifHandle,
                        0,
                        i,
                        out info);


                if (!result)
                {
                    break;
                }


                string type;


                switch (info.PipeType)
                {
                    case UsbdPipeTypeControl:
                        type = "CONTROL";
                        break;

                    case UsbdPipeTypeIsochronous:
                        type = "ISO";
                        break;

                    case UsbdPipeTypeBulk:
                        type = "BULK";
                        break;

                    case UsbdPipeTypeInterrupt:
                        type = "INTERRUPT";
                        break;

                    default:
                        type = "UNKNOWN";
                        break;
                }


                Status(
                    string.Format(
                        "PIPE[{0}] id=0x{1:X2} type={2} maxPacket={3} interval={4}",
                        i,
                        info.PipeId,
                        type,
                        info.MaximumPacketSize,
                        info.Interval));
            }


            Status(
                "==========================================");
        }


        // ============================================================
        // Set RAW_IO
        // ============================================================

        private void SetRawIo(
            byte pipeId,
            string name)
        {
            byte raw = 1;


            bool result =
                WinUsb_SetPipePolicy(
                    _ifHandle,
                    pipeId,
                    RAW_IO,
                    1,
                    ref raw);


            int err =
                Marshal.GetLastWin32Error();


            if (_verboseLog)
            {
                Status(
                    string.Format(
                        "RAW_IO {0}: result={1} err={2}",
                        name,
                        result,
                        err));
            }
        }


        // ============================================================
        // Create RX resources
        // ============================================================

        private bool CreateReceiveResources()
        {
            try
            {
                // ----------------------------------------------------
                // Native RX buffer
                // ----------------------------------------------------

                _rdBuffer =
                    Marshal.AllocHGlobal(
                        READ_BUFFER_SIZE);


                // ----------------------------------------------------
                // OVERLAPPED
                // ----------------------------------------------------

                int ovSize =
                    Marshal.SizeOf(
                        typeof(OVERLAPPED));


                _rdOvPtr =
                    Marshal.AllocHGlobal(
                        ovSize);


                // 清零
                for (int i = 0; i < ovSize; i++)
                {
                    Marshal.WriteByte(
                        _rdOvPtr,
                        i,
                        0);
                }


                // ----------------------------------------------------
                // Event
                // ----------------------------------------------------

                _rdEvent =
                    CreateEvent(
                        IntPtr.Zero,
                        false,
                        false,
                        null);


                if (_rdEvent == IntPtr.Zero)
                {
                    Status(
                        "CreateEvent failed, err=" +
                        Marshal.GetLastWin32Error());

                    return false;
                }


                // ----------------------------------------------------
                // OVERLAPPED.hEvent
                // ----------------------------------------------------

                OVERLAPPED ov =
                    new OVERLAPPED();

                ov.hEvent =
                    _rdEvent;


                Marshal.StructureToPtr(
                    ov,
                    _rdOvPtr,
                    false);


                if (_verboseLog)
                {
                    Status(
                        "WinUSB: RX resources created");
                }


                return true;
            }
            catch (Exception ex)
            {
                Status(
                    "CreateReceiveResources exception: " +
                    ex.Message);

                return false;
            }
        }


        // ============================================================
        // RX Thread
        // ============================================================

        private void ReceiveThreadProc()
        {
            if (_verboseLog)
            {
                Status(
                    "WinUSB RX thread started");
            }


            while (_receiveRunning)
            {
                try
                {
                    ReceiveOneTransfer();
                }
                catch (Exception ex)
                {
                    Interlocked.Increment(
                        ref _rxErrorCount);


                    Status(
                        "RX exception: " +
                        ex.Message);


                    Thread.Sleep(10);
                }
            }


            Status(
                string.Format(
                    "RX statistics: submit={0}, pending={1}, complete={2}, error={3}, bytes={4}",
                    _rxSubmitCount,
                    _rxPendingCount,
                    _rxCompleteCount,
                    _rxErrorCount,
                    _rxBytes));


            Status(
                "WinUSB RX thread stopped");
        }


        // ============================================================
        // One blocking RX transfer (always-pending read)
        // ============================================================

        private void ReceiveOneTransfer()
        {
            if (!_receiveRunning)
                return;

            if (!_opened)
                return;

            if (_ifHandle == IntPtr.Zero)
                return;

            if (_rdBuffer == IntPtr.Zero)
                return;

            // --------------------------------------------------------
            // Blocking read with infinite timeout.
            // PIPE_TRANSFER_TIMEOUT=0 keeps the URB pending until the
            // device sends data (with RAW_IO a short packet completes
            // immediately), so this thread always has one outstanding
            // read -> no sparse-IN polling gap.
            // --------------------------------------------------------

            long submitTick =
                _sw.ElapsedTicks;

            Interlocked.Increment(
                ref _rxSubmitCount);

            uint transferred;

            bool result =
                WinUsb_ReadPipe(
                    _ifHandle,
                    EP_IN,
                    _rdBuffer,
                    READ_BUFFER_SIZE,
                    out transferred,
                    IntPtr.Zero);

            if (!result)
            {
                int error =
                    Marshal.GetLastWin32Error();

                if (
                    error == ERROR_OPERATION_ABORTED ||
                    !_receiveRunning)
                {
                    return;
                }

                Interlocked.Increment(
                    ref _rxErrorCount);

                // ----------------------------------------------------
                // Error backoff: device gone / re-enumerating produces a
                // flood of err=22. Log first + every 100th, sleep longer.
                // ----------------------------------------------------

                _rxErrorStreak++;

                bool deviceGone =
                    error == 22 ||   /* ERROR_BAD_COMMAND: pipe gone */
                    error == 1167 || /* ERROR_DEVICE_NOT_CONNECTED */
                    error == 31 ||   /* ERROR_GEN_FAILURE */
                    error == 433 ||  /* USBD error: device re-enum */
                    error == 121;    /* ERROR_SEM_TIMEOUT */

                if (
                    _rxErrorStreak == 1 ||
                    (_rxErrorStreak % 100) == 0)
                {
                    Status(
                        string.Format(
                            "RX ERROR ReadPipe err={0} (x{1})",
                            error,
                            _rxErrorStreak));
                }

                Thread.Sleep(deviceGone ? 250 : 10);

                return;
            }

            _rxErrorStreak = 0;

            double elapsed =
                TicksToMilliseconds(
                    _sw.ElapsedTicks -
                    submitTick);

            Interlocked.Increment(
                ref _rxCompleteCount);

            if (transferred > 0)
            {
                Interlocked.Add(
                    ref _rxBytes,
                    transferred);
            }

            if (transferred > 0)
            {
                /* v9.82: 必须先入队（ReadAck 依赖队列），再打日志——
                 * Status 是同步 UI 封送，UI 忙会阻塞 RX 线程，先入队保证
                 * ACK 立即送达上层，日志晚点无所谓 */
                ProcessReceivedData(
                    transferred);
            }

            if (transferred > 1 && _verboseLog)   /* 保活 1B 不刷；简洁模式不打逐 ACK */
            {
                Status(
                    string.Format(
                        "RX COMPLETE bytes={0} elapsed={1:F3}ms",
                        transferred,
                        elapsed));
            }
        }


        // ============================================================
        // Tick -> ms
        // ============================================================

        private static double TicksToMilliseconds(
            long ticks)
        {
            return
                ticks * 1000.0 /
                Stopwatch.Frequency;
        }


        // ============================================================
        // Process received USB data
        // ============================================================

        private void ProcessReceivedData(
            uint length)
        {
            if (length == 0)
                return;


            if (length > READ_BUFFER_SIZE)
            {
                Status(
                    "RX invalid length=" +
                    length);

                return;
            }

            /* v9.82: 设备保活填充（1 字节 0x00）——静默丢弃，不入队不投递不刷日志 */
            if (
                length == 1 &&
                Marshal.ReadByte(_rdBuffer, 0) == 0x00)
            {
                return;
            }


            byte[] data =
                new byte[length];


            Marshal.Copy(
                _rdBuffer,
                data,
                0,
                (int)length);


            // --------------------------------------------------------
            // Enqueue for legacy Read() consumers
            // --------------------------------------------------------

            lock (_rxLock)
            {
                for (int i = 0; i < data.Length; i++)
                {
                    _rxQueue.Enqueue(data[i]);
                }
            }


            // --------------------------------------------------------
            // Deliver to application
            // --------------------------------------------------------

            try
            {
                DataReceived?.Invoke(
                    data,
                    (int)length);
            }
            catch (Exception ex)
            {
                Status(
                    "DataReceived exception: " +
                    ex.Message);
            }
        }


        // ============================================================
        // Write EP4 OUT
        // ============================================================

        public bool Write(
            byte[] data,
            int offset,
            int count)
        {
            if (!_opened)
                return false;


            if (_ifHandle == IntPtr.Zero)
                return false;


            if (data == null)
                return false;


            if (
                offset < 0 ||
                count < 0 ||
                offset + count > data.Length)
            {
                return false;
            }


            byte[] sendBuffer;


            if (
                offset == 0 &&
                count == data.Length)
            {
                sendBuffer = data;
            }
            else
            {
                sendBuffer =
                    new byte[count];


                Buffer.BlockCopy(
                    data,
                    offset,
                    sendBuffer,
                    0,
                    count);
            }


            uint transferred;


            bool result =
                WinUsb_WritePipe(
                    _ifHandle,
                    EP_OUT,
                    sendBuffer,
                    (uint)count,
                    out transferred,
                    IntPtr.Zero);


            if (!result)
            {
                int err =
                    Marshal.GetLastWin32Error();


                Status(
                    string.Format(
                        "TX ERROR err={0} len={1}",
                        err,
                        count));


                return false;
            }


            return transferred == count;
        }


        // ============================================================
        // Read - legacy queue drain (non-blocking)
        // ============================================================

        /// <summary>
        /// Copies up to count bytes from the internal RX queue.
        /// Returns bytes copied; 0 = no data; -1 = closed / invalid args.
        /// </summary>
        public int Read(
            byte[] buffer,
            int offset,
            int count)
        {
            if (
                !_opened ||
                buffer == null ||
                offset < 0 ||
                count < 0 ||
                offset + count > buffer.Length)
            {
                return -1;
            }

            if (count == 0)
            {
                return 0;
            }

            int copied = 0;

            lock (_rxLock)
            {
                int n = Math.Min(count, _rxQueue.Count);

                for (int i = 0; i < n; i++)
                {
                    buffer[offset + i] =
                        _rxQueue.Dequeue();
                }

                copied = n;
            }

            return copied;
        }


        // ============================================================
        // Close
        // ============================================================

        public void Close()
        {
            // --------------------------------------------------------
            // Stop RX
            // --------------------------------------------------------

            _receiveRunning = false;


            // --------------------------------------------------------
            // Cancel pending RX
            // --------------------------------------------------------

            if (_devHandle != IntPtr.Zero)
            {
                try
                {
                    bool result =
                        CancelIoEx(
                            _devHandle,
                            IntPtr.Zero);


                    int err =
                        Marshal.GetLastWin32Error();


                    if (_verboseLog)
                    {
                        Status(
                            string.Format(
                                "CancelIoEx result={0} err={1}",
                                result,
                                err));
                    }
                }
                catch
                {
                }
            }


            // --------------------------------------------------------
            // Wait RX thread
            // --------------------------------------------------------

            if (
                _receiveThread != null &&
                _receiveThread != Thread.CurrentThread)
            {
                try
                {
                    _receiveThread.Join(
                        1000);
                }
                catch
                {
                }
            }


            _receiveThread = null;


            // --------------------------------------------------------
            // Mark closed
            // --------------------------------------------------------

            _opened = false;


            // --------------------------------------------------------
            // Free WinUSB
            // --------------------------------------------------------

            if (_ifHandle != IntPtr.Zero)
            {
                try
                {
                    WinUsb_Free(
                        _ifHandle);
                }
                catch
                {
                }


                _ifHandle =
                    IntPtr.Zero;
            }


            // --------------------------------------------------------
            // Close device handle
            // --------------------------------------------------------

            if (_devHandle != IntPtr.Zero)
            {
                try
                {
                    CloseHandle(
                        _devHandle);
                }
                catch
                {
                }


                _devHandle =
                    IntPtr.Zero;
            }


            // --------------------------------------------------------
            // Close RX event
            // --------------------------------------------------------

            if (_rdEvent != IntPtr.Zero)
            {
                try
                {
                    CloseHandle(
                        _rdEvent);
                }
                catch
                {
                }


                _rdEvent =
                    IntPtr.Zero;
            }


            // --------------------------------------------------------
            // Free OVERLAPPED
            // --------------------------------------------------------

            if (_rdOvPtr != IntPtr.Zero)
            {
                try
                {
                    Marshal.FreeHGlobal(
                        _rdOvPtr);
                }
                catch
                {
                }


                _rdOvPtr =
                    IntPtr.Zero;
            }


            // --------------------------------------------------------
            // Free RX buffer
            // --------------------------------------------------------

            if (_rdBuffer != IntPtr.Zero)
            {
                try
                {
                    Marshal.FreeHGlobal(
                        _rdBuffer);
                }
                catch
                {
                }


                _rdBuffer =
                    IntPtr.Zero;
            }


            if (_verboseLog)
            {
                Status(
                    "WinUSB: closed");
            }
        }


        // ============================================================
        // Dispose
        // ============================================================

        public void Dispose()
        {
            Close();
        }
    }
}