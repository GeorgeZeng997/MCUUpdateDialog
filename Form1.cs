using System;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using HidLibrary;

namespace Fosi_update
{
    public partial class Form1 : Form
    {
        private const int VendorId = 0x152A; // 替换为你的设备 Vendor ID
        private const int ProductId = 0x889B; // 替换为你的设备 Product ID

        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            try
            {
                OpenFileDialog openFileDialog = new OpenFileDialog
                {
                    Filter = "Firmware Files (*.bin)|*.bin|All Files (*.*)|*.*",
                    Title = "Select a Firmware File"
                };

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    string filePath = openFileDialog.FileName;
                    textBox1.Text = filePath;
                    byte[] firmwareData = File.ReadAllBytes(filePath);
                    MessageBox.Show($"Update file size: {firmwareData.Length}\nUpdate pack size: {firmwareData.Length / 30 + 2}", "Information");
                }
                else
                {
                    MessageBox.Show("No file selected", "Warning");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"An error occurred: {ex.Message}", "Error");
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            try
            {
                if (string.IsNullOrEmpty(textBox1.Text))
                {
                    MessageBox.Show("Please select a firmware file first.", "Warning");
                    return;
                }

                byte[] firmwareData = File.ReadAllBytes(textBox1.Text);

                // 查找设备
                var device = HidDevices.Enumerate(VendorId, ProductId).FirstOrDefault();
                if (device == null)
                {
                    MessageBox.Show("No HID device detected.", "Error");
                    return;
                }

                device.OpenDevice();

                // 初始化数据包
                const int packetSize = 36;
                byte[] packet = new byte[41]; // 包含头部和校验和
                packet[0] = 0x01; // 示例协议头
                packet[1] = 0xAA;
                packet[2] = 0x55;

                int offset = 0;
                while (offset < firmwareData.Length)
                {
                    int dataLength = Math.Min(packetSize, firmwareData.Length - offset);
                    Array.Copy(firmwareData, offset, packet, 3, dataLength);

                    // 填充剩余部分
                    for (int i = 3 + dataLength; i < 39; i++)
                    {
                        packet[i] = 0x00;
                    }

                    // 计算校验和
                    ushort checksum = CalculateCRC(packet, 1, 39);
                    packet[39] = (byte)(checksum & 0xFF);
                    packet[40] = (byte)((checksum >> 8) & 0xFF);

                    // 写入 HID 数据
                    if (!device.WriteFeatureData(packet))
                    {
                        MessageBox.Show($"Data transfer failed at offset {offset}.", "Error");
                        break;
                    }

                    offset += dataLength;
                }

                device.CloseDevice();

                if (offset == firmwareData.Length)
                {
                    MessageBox.Show("Firmware upgrade completed!", "Success");
                }
                else
                {
                    MessageBox.Show("Firmware upgrade failed!", "Fail");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"An error occurred: {ex.Message}", "Error");
            }
        }

        private ushort CalculateCRC(byte[] data, int offset, int length)
        {
            ushort crc = 0xFFFF;
            for (int i = offset; i < offset + length; i++)
            {
                crc ^= data[i];
                for (int j = 0; j < 8; j++)
                {
                    if ((crc & 0x0001) != 0)
                    {
                        crc >>= 1;
                        crc ^= 0xA001;
                    }
                    else
                    {
                        crc >>= 1;
                    }
                }
            }
            return crc;
        }
    }
}
