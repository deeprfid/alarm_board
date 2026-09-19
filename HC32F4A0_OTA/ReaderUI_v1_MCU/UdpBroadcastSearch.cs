using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ReaderManager
{
    internal class UdpBroadcastSearch
    {
        private static readonly int BroadcastPort = 11111;
        private static readonly int Timeout = 2000; // 超时时间（毫秒）
        public bool isClosing { get; private set; }

        private static UdpClient myUdp;
        public void StartUdp()
        {
            isClosing = false;
            myUdp = new UdpClient(BroadcastPort);

            IPEndPoint localEndPoint = myUdp.Client.LocalEndPoint as IPEndPoint;
            IPAddress LocalAddress  = localEndPoint.Address;
            var serverEndpoint = new IPEndPoint(LocalAddress, BroadcastPort);

            myUdp.EnableBroadcast = true;
            myUdp.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, 1);
            myUdp.Client.Bind(serverEndpoint);
            myUdp.BeginReceive(ReceiveCallback, myUdp);

         /*
             UdpClient broadcastClient = new UdpClient();

            IPEndPoint broadcastEp = new IPEndPoint(IPAddress.Broadcast, BroadcastPort);
            IPEndPoint remoteEp = new IPEndPoint(IPAddress.Any, 0);
            byte[] broadcastData = Encoding.ASCII.GetBytes("Discovery");

            UdpClient client = new UdpClient(AddressFamily.InterNetwork);
            client.EnableBroadcast = true;
            client.Send(request, request.Length, broadcastEndPoint);
            client.BeginReceive(Callback, client);


            broadcastClient.EnableBroadcast = true;
            broadcastClient.Send(broadcastData, broadcastData.Length, broadcastEp);

            Console.WriteLine("Waiting for devices...");

            while (true)
            {
                try
                {
                    // 接收响应，使用超时来避免阻塞
                    broadcastClient.Client.ReceiveTimeout = Timeout;
                    byte[] receivedData = broadcastClient.Receive(ref remoteEp);
                    string receivedMessage = Encoding.ASCII.GetString(receivedData);

                    Console.WriteLine($"Received message from {remoteEp.Address}: {receivedMessage}");
                }
                catch (SocketException ex) when (ex.SocketErrorCode == SocketError.TimedOut)
                {
                    // 超时异常，继续循环等待
                    continue;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error receiving broadcast: {ex.Message}");
                    break;
                }
            }

            broadcastClient.Close();*/
        }

        private  void ReceiveCallback(IAsyncResult asyncResult)
        {
            if (isClosing)
                return;

            UdpClient client = (UdpClient)asyncResult.AsyncState;
            IPEndPoint remoteEndPoint = null;
            byte[] receivedData = client.EndReceive(asyncResult, ref remoteEndPoint);
            string receivedMessage = Encoding.ASCII.GetString(receivedData);

            // Do something with response

            if (!isClosing && client != null)
            {
                client.BeginReceive(ReceiveCallback, client);
            }
   
        }
        public void SendBroad()
        {
            IPEndPoint broadcastEp = new IPEndPoint(IPAddress.Broadcast, BroadcastPort);
            IPEndPoint remoteEp = new IPEndPoint(IPAddress.Any, 0);
            byte[] broadcastData = Encoding.ASCII.GetBytes("Discovery");

            myUdp.EnableBroadcast = true;
            myUdp.Send(broadcastData, broadcastData.Length, broadcastEp);

            Console.WriteLine("Waiting for devices...");
        }
    }
}
