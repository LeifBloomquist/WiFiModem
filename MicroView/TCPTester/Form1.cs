using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TCPTester
{
    public partial class Form1 : Form
    {
        TcpClient client = null;
        NetworkStream stream = null;

        public Form1()
        {
            InitializeComponent();
        }

        private void ConnectButton_Click(object sender, EventArgs e)
        {
            try
            {
                // Create a TcpClient.
                // Note, for this client to work you need to have a TcpServer 
                // connected to the same address as specified by the server, port
                // combination.
                Int32 port = 2004;
                client = new TcpClient(IPAddress.Text, port);

                // Translate the passed message into ASCII and store it as a Byte array.
              

                stream = client.GetStream();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Exception: " + ex.Message);
                return;
            }
        }
      

        private void SendData_Click(object sender, EventArgs e)
        {
            if (client == null) return;
            if (stream == null) return;

            String message = "";
            String line = "123456789\r";

            for (int i=0; i<1000; i++)
            {
                message += (int)i + ":" + line;
            }

            Byte[] data = System.Text.Encoding.ASCII.GetBytes(message);
            stream.Write(data, 0, data.Length);        
        }

       private void DoDisconnect()
       {
            // Close everything.
            stream.Close();
            client.Close();
        }

       private void Disconnect_Click(object sender, EventArgs e)
       {
           DoDisconnect();
       }
    }
}
