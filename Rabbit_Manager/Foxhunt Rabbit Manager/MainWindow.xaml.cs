using System;
using System.IO.Ports;
using System.Timers;
using System.Windows;
using System.Windows.Controls;

namespace FoxhuntRabbitManager
{
    public partial class MainWindow : Window
    {
        private bool gPortHandle = true;
        private SerialPort sPort;

        readonly private Timer msgTimer = new Timer();
        readonly private Timer portTimer = new Timer();
        readonly private Timer reqTimer = new Timer();

        readonly byte preval = 0x7E;
        readonly private byte cwbit = 0, altbit = 1, pwrbit = 2, huntbit = 3, pttbit = 4, rndbit = 5, autobit = 6;

        string pName;
        bool disConn = false, isStarted = false, isClosed = false, ctrlRqst = false;
        uint portSeconds, firstLoad = 1, currentControls = 0;
        readonly uint portTimeout = 15;

        public MainWindow()
        {

            InitializeComponent();

            LoadPorts();

            ComPortsComboBox.SelectedIndex = -1;

            msgTimer.Elapsed += new ElapsedEventHandler(ClearMsg);
            msgTimer.Interval = 10000;

            portTimer.Elapsed += new ElapsedEventHandler(CheckPort);
            portTimer.Interval = 1000;
            portTimer.Start();

            reqTimer.Elapsed += new ElapsedEventHandler(DoRequestControls);
            reqTimer.Interval = 250;
        }
        private void MainWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (((currentControls >> huntbit) & 1) == 1)
            {
                MessageBox.Show("You are running a hunt. The rabbit will not stop transmitting.\r\n" +
                    "To stop the hunt, restart the software and click Stop Hunt.", "WARNING!");
            }
        }
        private void DoRequestControls(object sender, ElapsedEventArgs e)
        {
            RequestControls();
            reqTimer.Stop();
        }
        private void CheckPort(object sender, ElapsedEventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            bool exists = false;

            LoadPorts();

            if (!isStarted)
                return;

            if (ctrlRqst)
            {
                this.Dispatcher.Invoke(() =>
                {
                    msgBox.Text = "No controls received from the rabbit. Trying again.";
                    DisableAll();
                    RequestControls();
                });
            }

            portSeconds++;
            foreach (string port in ports)
            {
                if (port == pName)
                    exists = true;
            }
            this.Dispatcher.Invoke(() =>
            {

                if (portSeconds >= portTimeout && !disConn)
                {
                    msgBox.Text = "Rabbit ping timeout: It appears the WiFi connection is down";
                    pingBox.Text = "--- ms";
                    DisableAll();
                    /*
                     * one exception to always be able to power off rule,
                     * since we can't talk anyway
                     */
                    pwrButton.IsEnabled = false;
                    portSeconds = portTimeout;
                }

                if (disConn)
                    return;

                if (!exists && !isClosed)
                {
                    msgBox.Text = "Rabbit remote diconnected";
                    pingBox.Text = "--- ms";
                    sPort.Close();
                    AttachButton.IsEnabled = false;
                    DettachButton.IsEnabled = false;
                    ComPortsComboBox.IsEnabled = true;
                    disConn = true;
                    isClosed = true;
                    isStarted = false;
                }
            });
        }
        private void ClearMsg(object sender, ElapsedEventArgs e)
        {
            this.Dispatcher.Invoke(() =>
            {
                if (portSeconds < portTimeout && !disConn)
                    msgBox.Text = "";
            });
        }
        private void LoadPorts()
        {
            string[] ports = SerialPort.GetPortNames();
            uint count = 0, reload = 0;
            int i = 0;

            foreach (string item in ComPortsComboBox.Items)
                count++;

            if (ports.Length != ComPortsComboBox.Items.Count)
                goto load;

            foreach (string item in ComPortsComboBox.Items)
            {
                if (ports[i] != ComPortsComboBox.Items.GetItemAt(i++).ToString())
                    reload = 1;
            }

            if (firstLoad == 1)
                goto load;
            if (reload == 0)
                return;
            load:
            firstLoad = 0;
            this.Dispatcher.Invoke(() =>
            {
                ComPortsComboBox.Items.Clear();
                foreach (string port in ports)
                {
                    ComPortsComboBox.Items.Add(port);
                }
                msgBox.Text = "Select a com port to connect to the rabbit remote";
            });
        }
        private void GetPortClosed(object sender, EventArgs e)
        {
            if (gPortHandle) GetPortHandle();
            gPortHandle = true;
        }
        private void GetPortChanged(object sender, EventArgs e)
        {
            ComboBox cmb = sender as ComboBox;
            gPortHandle = !cmb.IsDropDownOpen;
            GetPortHandle();
        }
        private void GetPortHandle()
        {
            if (ComPortsComboBox.SelectedIndex != -1)
            {
                AttachButton.IsEnabled = true;
            }
            else
            {
                AttachButton.IsEnabled = false;
            }
        }
        private void AttachPortHandle(object sender, EventArgs e)
        {
            msgBox.Text = "Connecting to the rabbit remote";
            AttachPort();
            AttachButton.IsEnabled = false;
            ComPortsComboBox.IsEnabled = false;
            DettachButton.IsEnabled = true;

        }
        private void AttachPort()
        {
            pName = ComPortsComboBox.SelectedValue.ToString();
            sPort = new SerialPort(pName, 115200, Parity.None, 8,
                StopBits.One);
            try
            {
                sPort.DataReceived += new
                    SerialDataReceivedEventHandler(ReceiveData);
                sPort.Open();
            }
            catch (Exception ex)
            {
                AttachButton.IsEnabled = true;
                ComPortsComboBox.IsEnabled = true;
                msgBox.Text = "Connection to the rabbit remote failed";
                MessageBox.Show("Error: " + ex.ToString(), "ERROR");
                return;
            }

            DettachButton.IsEnabled = true;

            msgBox.Text = "Connected to the rabbit remote";
            pingBox.Text = "--- ms";
            portSeconds = 0;
            msgTimer.Start();
            disConn = false;
            isStarted = true;
            isClosed = false;

            RequestControls();
        }
        private void RequestControls()
        {
            byte[] buf = new byte[6];
            int i = 0, j;
            int sum = 0;
            byte fin;

            buf[i++] = preval;
            buf[i++] = 0x04;
            buf[i++] = 1;
            buf[i++] = 0;

            for (j = 0; j < i; j++)
                sum += buf[j];
            fin = (byte)(sum & 0xFF);

            buf[i++] = fin;

            if (sPort.IsOpen)
                sPort.Write(buf, 0, 6);

            ctrlRqst = true;
        }
        private void DettachPortHandle(object sender, EventArgs e)
        {
            DettachPort();
            msgBox.Text = "Disconnected from the rabbit remote";
            pingBox.Text = "--- ms";
            AttachButton.IsEnabled = true;
            DettachButton.IsEnabled = false;
            ComPortsComboBox.IsEnabled = true;

            DisableAll();
            msgTimer.Stop();
            isStarted = false;
        }
        private void EnableAll()
        {
            pwrButton.IsEnabled = true;
            huntButton.IsEnabled = true;
            pttButton.IsEnabled = true;
            delayBox.IsEnabled = true;
            pwrButton.IsEnabled = true;
            altButton.IsEnabled = true;
            cwButton.IsEnabled = true;
            rndButton.IsEnabled = true;
            delayBox.IsEnabled = true;
            setButton.IsEnabled = true;
            autoButton.IsEnabled = true;
        }
        private void DisableAll()
        {
            pwrButton.IsEnabled = false;
            huntButton.IsEnabled = false;
            pttButton.IsEnabled = false;
            delayBox.IsEnabled = false;
            /* we always have the option to turn the trasmitter off */
            pwrButton.IsEnabled = true;
            altButton.IsEnabled = false;
            cwButton.IsEnabled = false;
            rndButton.IsEnabled = false;
            delayBox.IsEnabled = false;
            setButton.IsEnabled = false;
            autoButton.IsEnabled = false;
        }
        private void DettachPort()
        {
            try
            {
                sPort.Close();
            }
            catch (Exception ex)
            {
                AttachButton.IsEnabled = false;
                DettachButton.IsEnabled = true;
                ComPortsComboBox.IsEnabled = false;
                MessageBox.Show("Error: " + ex.ToString(), "ERROR");
            }
        }
        private void CwHandle(object sender, RoutedEventArgs e)
        {
            SendControls(cwbit);
        }
        private void AltHandle(object sender, RoutedEventArgs e)
        {
            SendControls(altbit);
        }
        private void PwrHandle(object sender, RoutedEventArgs e)
        {
            SendControls(pwrbit);
        }
        private void HuntHandle(object sender, RoutedEventArgs e)
        {
            SendControls(huntbit);
        }
        private void AutoHandle(object sender, RoutedEventArgs e)
        {
            SendControls(autobit);
        }
        private void PttHandle(object sender, RoutedEventArgs e)
        {
            SendControls(pttbit);
        }
        private void RndHandle(object sender, RoutedEventArgs e)
        {
            SendControls(rndbit);
        }
        private void SetHandle(object sender, RoutedEventArgs e)
        {
            byte[] buf = new byte[6];
            int i = 0, j;
            int sum = 0;
            byte fin;

            buf[i++] = preval;
            buf[i++] = 0x02;
            buf[i++] = 1;
            buf[i++] = byte.Parse(delayBox.Text);

            for (j = 0; j < i; j++)
                sum += buf[j];
            fin = (byte)(sum & 0xFF);

            buf[i++] = fin;

            if (sPort.IsOpen)
                sPort.Write(buf, 0, 6);

            DisableAll();
            reqTimer.Start();
        }
        private void SendControls(byte bit)
        {
            currentControls ^= (uint)(1 << bit);

            if (bit == altbit && ((currentControls >> cwbit) & 1) == 0)
                currentControls ^= (uint)(1 << cwbit);
            if (bit == pwrbit && ((currentControls >> huntbit) & 1) == 1)
                currentControls ^= (uint)(1 << huntbit);

            byte[] buf = new byte[6];
            int i = 0, j;
            int sum = 0;
            byte fin;

            buf[i++] = preval;
            buf[i++] = 0x03;
            buf[i++] = 1;
            buf[i++] = (byte)currentControls;

            for (j = 0; j < i; j++)
                sum += buf[j];
            fin = (byte)(sum & 0xFF);

            buf[i++] = fin;

            if (sPort.IsOpen)
                sPort.Write(buf, 0, 6);

            DisableAll();
            if (bit == huntbit && ((currentControls >> huntbit) & 1) == 1)
            {
                huntButton.Content = "Stop Hunt";
                DettachButton.IsEnabled = false;
                return;
            }
            else if (bit == pttbit)
                return;
            else
                DettachButton.IsEnabled = true;

            reqTimer.Start();
        }
        private void ReceiveData(object sender,
            SerialDataReceivedEventArgs e)
        {
            SerialPort thisSp = (SerialPort)sender;
            int i, j, cnt = 0, cnt_req;
            byte[] pkt = new byte[255];

            for (i = 0; i < 3; i++)
            {
                pkt[i] = (byte)thisSp.ReadByte();
                cnt++;
            }
            j = i + pkt[2];
            for (i = 3; i < j; i++)
            {
                pkt[i] = (byte)thisSp.ReadByte();
                cnt++;
            }
            pkt[i] = (byte)thisSp.ReadByte();
            cnt++;

            cnt_req = 4 + pkt[2];

            this.Dispatcher.Invoke(() =>
            {
                if (pkt[0] != preval)
                    msgBox.Text = "Preamble error";
                else if (cnt != cnt_req)
                    msgBox.Text = "Data RX error: " + cnt.ToString() + "::" + cnt_req.ToString();
                else
                    ParseData(pkt);
            });
        }
        private void UpdateControls(uint data)
        {
            currentControls = data;

            uint cw, alt, pwr, hunt, ptt, rnd, auto;

            cw = (currentControls >> cwbit) & 1;
            alt = (currentControls >> altbit) & 1;
            pwr = (currentControls >> pwrbit) & 1;
            hunt = (currentControls >> huntbit) & 1;
            ptt = (currentControls >> pttbit) & 1;
            rnd = (currentControls >> rndbit) & 1;
            auto = (currentControls >> autobit) & 1;

            EnableAll();

            if (auto == 1)
                autoButton.Content = "Disable Auto Hunt";
            else
                autoButton.Content = "Enable Auto Hunt";

            if (hunt == 1)
            {
                huntButton.Content = "Stop Hunt";
                pttButton.IsEnabled = true;
                DettachButton.IsEnabled = false;
            }
            else
            {
                huntButton.Content = "Start Hunt";
                pttButton.IsEnabled = false;
                DettachButton.IsEnabled = true;
            }

            if (ptt == 1)
            {
                huntButton.IsEnabled = false;
                pttButton.IsEnabled = false;
            }

            if (pwr == 1)
            {
                pwrButton.Content = "Transmitter Power Off";
                huntButton.IsEnabled = true;
                if (hunt == 1)
                    pttButton.IsEnabled = true;
            }
            else
            {
                pwrButton.Content = "Transmitter Power On";
                huntButton.IsEnabled = false;
                pttButton.IsEnabled = false;
            }

            if (rnd == 1)
            {
                rndButton.Content = "Turn Off Random Delay";
                delayBox.IsEnabled = false;
                setButton.IsEnabled = false;
            }
            else
            {
                rndButton.Content = "Turn On Random Delay";
                delayBox.IsEnabled = true;
                setButton.IsEnabled = true;
            }

            if (cw == 1)
                cwButton.Content = "Turn Off CW";
            else
                cwButton.Content = "Turn On CW";

            if (alt == 1)
            {
                altButton.Content = "Turn Off CW/Voice Alternator";
                cwButton.Content = "Turn Off CW";
                cwButton.IsEnabled = false;
            }
            else
            {
                altButton.Content = "Turn On CW/Voice Alternator";
                cwButton.IsEnabled = true;
            }
        }
        private void ParseData(byte[] pkt)
        {
            /* build our data */
            int h, i = 0, j, pre, type, cnt, calc, sum, fin;
            uint data = 0;

            pre = pkt[i++];
            type = pkt[i++];
            cnt = pkt[i++];

            j = (cnt - 1) * 8;

            for (h = 0; h < cnt; h++)
            {
                if (j > 0)
                    data |= (uint)(pkt[i++] << j);
                else
                    data |= (uint)pkt[i++];
                j -= 8;
            }

            calc = pkt[i++];

            sum = pre + type + cnt + (int)data;
            fin = sum & 0xFF;

            if (fin != calc)
            {
                msgBox.Text = "Checksum error: " + calc.ToString("X2") + "::" + fin.ToString("X2");
                return;
            }

            switch (type)
            {
                case 0x01: // ping request
                    if (portSeconds >= portTimeout)
                    {
                        msgBox.Text = "Rabbit WiFi connection reestablished";
                        RequestControls();
                    }
                    portSeconds = 0;
                    pingBox.Text = data + " ms";
                    break;
                case 0x02: // transmit deley
                    delayBox.Text = data.ToString();
                    break;
                case 0x04: // control request
                    UpdateControls(data);
                    ctrlRqst = false;
                    break;
                case 0x05: // enable controls after transmission
                    RequestControls();
                    break;
                case 0x06: // disable controls
                    DisableAll();
                    msgBox.Text = "Controls disabled during transmission";
                    break;
                default:
                    break;
            }
        }
    }
}