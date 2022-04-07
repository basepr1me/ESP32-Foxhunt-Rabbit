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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "No controls received from the rabbit. Trying again.";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "Rabbit ping timeout: It appears the WiFi connection is down";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'pingBox' does not exist in the current context
                    pingBox.Text = "--- ms";
#pragma warning restore CS0103 // The name 'pingBox' does not exist in the current context
                    DisableAll();
                    /*
                     * one exception to always be able to power off rule,
                     * since we can't talk anyway
                     */
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
                    pwrButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
                    portSeconds = portTimeout;
                }

                if (disConn)
                    return;

                if (!exists && !isClosed)
                {
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "Rabbit remote diconnected";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'pingBox' does not exist in the current context
                    pingBox.Text = "--- ms";
#pragma warning restore CS0103 // The name 'pingBox' does not exist in the current context
                    sPort.Close();
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
                    AttachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                    DettachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                    ComPortsComboBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
            });
        }
        private void LoadPorts()
        {
            string[] ports = SerialPort.GetPortNames();
            uint count = 0, reload = 0;
            int i = 0;

#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            foreach (string item in ComPortsComboBox.Items)
                count++;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context

#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            if (ports.Length != ComPortsComboBox.Items.Count)
                goto load;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context

#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            foreach (string item in ComPortsComboBox.Items)
            {
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                if (ports[i] != ComPortsComboBox.Items.GetItemAt(i++).ToString())
                    reload = 1;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            }
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context

            if (firstLoad == 1)
                goto load;
            if (reload == 0)
                return;
            load:
            firstLoad = 0;
            this.Dispatcher.Invoke(() =>
            {
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                ComPortsComboBox.Items.Clear();
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                foreach (string port in ports)
                {
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                    ComPortsComboBox.Items.Add(port);
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                }
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                msgBox.Text = "Select a com port to connect to the rabbit remote";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            if (ComPortsComboBox.SelectedIndex != -1)
            {
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
                AttachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
            }
            else
            {
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
                AttachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
            }
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
        }
        private void AttachPortHandle(object sender, EventArgs e)
        {
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
            msgBox.Text = "Connecting to the rabbit remote";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
            AttachPort();
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
            AttachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            ComPortsComboBox.IsEnabled = false;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
            DettachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context

        }
        private void AttachPort()
        {
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            pName = ComPortsComboBox.SelectedValue.ToString();
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
                AttachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                ComPortsComboBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                msgBox.Text = "Connection to the rabbit remote failed";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
                MessageBox.Show("Error: " + ex.ToString(), "ERROR");
                return;
            }

#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
            DettachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context

#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
            msgBox.Text = "Connected to the rabbit remote";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'pingBox' does not exist in the current context
            pingBox.Text = "--- ms";
#pragma warning restore CS0103 // The name 'pingBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
            msgBox.Text = "Disconnected from the rabbit remote";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'pingBox' does not exist in the current context
            pingBox.Text = "--- ms";
#pragma warning restore CS0103 // The name 'pingBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
            AttachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
            DettachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
            ComPortsComboBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context

            DisableAll();
            msgTimer.Stop();
            isStarted = false;
        }
        private void EnableAll()
        {
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
            pwrButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
            huntButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
            pttButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
            delayBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
            pwrButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'altButton' does not exist in the current context
            altButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'altButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
            cwButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'rndButton' does not exist in the current context
            rndButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'rndButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
            delayBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'setButton' does not exist in the current context
            setButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'setButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'autoButton' does not exist in the current context
            autoButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'autoButton' does not exist in the current context
        }
        private void DisableAll()
        {
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
            pwrButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
            huntButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
            pttButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
            delayBox.IsEnabled = false;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
            /* we always have the option to turn the trasmitter off */
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
            pwrButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'altButton' does not exist in the current context
            altButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'altButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
            cwButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'rndButton' does not exist in the current context
            rndButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'rndButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
            delayBox.IsEnabled = false;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'setButton' does not exist in the current context
            setButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'setButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'autoButton' does not exist in the current context
            autoButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'autoButton' does not exist in the current context
        }
        private void DettachPort()
        {
            try
            {
                sPort.Close();
            }
            catch (Exception ex)
            {
#pragma warning disable CS0103 // The name 'AttachButton' does not exist in the current context
                AttachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'AttachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                DettachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'ComPortsComboBox' does not exist in the current context
                ComPortsComboBox.IsEnabled = false;
#pragma warning restore CS0103 // The name 'ComPortsComboBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
            buf[i++] = byte.Parse(delayBox.Text);
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context

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
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.Content = "Stop Hunt";
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                DettachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
                return;
            }
            else if (bit == pttbit)
                return;
            else
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                DettachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context

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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "Preamble error";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
                else if (cnt != cnt_req)
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "Data RX error: " + cnt.ToString() + "::" + cnt_req.ToString();
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'autoButton' does not exist in the current context
                autoButton.Content = "Disable Auto Hunt";
#pragma warning restore CS0103 // The name 'autoButton' does not exist in the current context
            else
#pragma warning disable CS0103 // The name 'autoButton' does not exist in the current context
                autoButton.Content = "Enable Auto Hunt";
#pragma warning restore CS0103 // The name 'autoButton' does not exist in the current context

            if (hunt == 1)
            {
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.Content = "Stop Hunt";
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
                pttButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                DettachButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
            }
            else
            {
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.Content = "Start Hunt";
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
                pttButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'DettachButton' does not exist in the current context
                DettachButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'DettachButton' does not exist in the current context
            }

            if (ptt == 1)
            {
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
                pttButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
            }

            if (pwr == 1)
            {
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
                pwrButton.Content = "Transmitter Power Off";
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
                if (hunt == 1)
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
                    pttButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
            }
            else
            {
#pragma warning disable CS0103 // The name 'pwrButton' does not exist in the current context
                pwrButton.Content = "Transmitter Power On";
#pragma warning restore CS0103 // The name 'pwrButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'huntButton' does not exist in the current context
                huntButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'huntButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'pttButton' does not exist in the current context
                pttButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'pttButton' does not exist in the current context
            }

            if (rnd == 1)
            {
#pragma warning disable CS0103 // The name 'rndButton' does not exist in the current context
                rndButton.Content = "Turn Off Random Delay";
#pragma warning restore CS0103 // The name 'rndButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
                delayBox.IsEnabled = false;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'setButton' does not exist in the current context
                setButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'setButton' does not exist in the current context
            }
            else
            {
#pragma warning disable CS0103 // The name 'rndButton' does not exist in the current context
                rndButton.Content = "Turn On Random Delay";
#pragma warning restore CS0103 // The name 'rndButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
                delayBox.IsEnabled = true;
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
#pragma warning disable CS0103 // The name 'setButton' does not exist in the current context
                setButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'setButton' does not exist in the current context
            }

            if (cw == 1)
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
                cwButton.Content = "Turn Off CW";
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
            else
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
                cwButton.Content = "Turn On CW";
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context

            if (alt == 1)
            {
#pragma warning disable CS0103 // The name 'altButton' does not exist in the current context
                altButton.Content = "Turn Off CW/Voice Alternator";
#pragma warning restore CS0103 // The name 'altButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
                cwButton.Content = "Turn Off CW";
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
                cwButton.IsEnabled = false;
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
            }
            else
            {
#pragma warning disable CS0103 // The name 'altButton' does not exist in the current context
                altButton.Content = "Turn On CW/Voice Alternator";
#pragma warning restore CS0103 // The name 'altButton' does not exist in the current context
#pragma warning disable CS0103 // The name 'cwButton' does not exist in the current context
                cwButton.IsEnabled = true;
#pragma warning restore CS0103 // The name 'cwButton' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                msgBox.Text = "Checksum error: " + calc.ToString("X2") + "::" + fin.ToString("X2");
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
                return;
            }

            switch (type)
            {
                case 0x01: // ping request
                    if (portSeconds >= portTimeout)
                    {
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                        msgBox.Text = "Rabbit WiFi connection reestablished";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
                        RequestControls();
                    }
                    portSeconds = 0;
#pragma warning disable CS0103 // The name 'pingBox' does not exist in the current context
                    pingBox.Text = data + " ms";
#pragma warning restore CS0103 // The name 'pingBox' does not exist in the current context
                    break;
                case 0x02: // transmit deley
#pragma warning disable CS0103 // The name 'delayBox' does not exist in the current context
                    delayBox.Text = data.ToString();
#pragma warning restore CS0103 // The name 'delayBox' does not exist in the current context
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
#pragma warning disable CS0103 // The name 'msgBox' does not exist in the current context
                    msgBox.Text = "Controls disabled during transmission";
#pragma warning restore CS0103 // The name 'msgBox' does not exist in the current context
                    break;
                default:
                    break;
            }
        }
    }
}