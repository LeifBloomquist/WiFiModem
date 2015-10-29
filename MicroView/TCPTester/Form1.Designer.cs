namespace TCPTester
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.IPAddress = new System.Windows.Forms.TextBox();
            this.ConnectButton = new System.Windows.Forms.Button();
            this.SendData = new System.Windows.Forms.Button();
            this.Disconnect = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // IPAddress
            // 
            this.IPAddress.Location = new System.Drawing.Point(12, 12);
            this.IPAddress.Name = "IPAddress";
            this.IPAddress.Size = new System.Drawing.Size(148, 20);
            this.IPAddress.TabIndex = 0;
            this.IPAddress.Text = "192.168.7.109";
            // 
            // ConnectButton
            // 
            this.ConnectButton.Location = new System.Drawing.Point(12, 38);
            this.ConnectButton.Name = "ConnectButton";
            this.ConnectButton.Size = new System.Drawing.Size(148, 23);
            this.ConnectButton.TabIndex = 1;
            this.ConnectButton.Text = "Connect";
            this.ConnectButton.UseVisualStyleBackColor = true;
            this.ConnectButton.Click += new System.EventHandler(this.ConnectButton_Click);
            // 
            // SendData
            // 
            this.SendData.Location = new System.Drawing.Point(12, 67);
            this.SendData.Name = "SendData";
            this.SendData.Size = new System.Drawing.Size(148, 43);
            this.SendData.TabIndex = 2;
            this.SendData.Text = "Send Data";
            this.SendData.UseVisualStyleBackColor = true;
            this.SendData.Click += new System.EventHandler(this.SendData_Click);
            // 
            // Disconnect
            // 
            this.Disconnect.Location = new System.Drawing.Point(12, 116);
            this.Disconnect.Name = "Disconnect";
            this.Disconnect.Size = new System.Drawing.Size(148, 23);
            this.Disconnect.TabIndex = 3;
            this.Disconnect.Text = "Disconnect";
            this.Disconnect.UseVisualStyleBackColor = true;
            this.Disconnect.Click += new System.EventHandler(this.Disconnect_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(175, 157);
            this.Controls.Add(this.Disconnect);
            this.Controls.Add(this.SendData);
            this.Controls.Add(this.ConnectButton);
            this.Controls.Add(this.IPAddress);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "Form1";
            this.Opacity = 0.9D;
            this.Text = "TCP Test Program";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox IPAddress;
        private System.Windows.Forms.Button ConnectButton;
        private System.Windows.Forms.Button SendData;
        private System.Windows.Forms.Button Disconnect;
    }
}

