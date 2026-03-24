#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char indexHtml[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <!-- <link rel="stylesheet" href="../css/style.css"> -->
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="EmbPr" content="width=device-width, initial-scale=1.0">
    <script src="/code.js"></script>
    <link rel="stylesheet" href="/style.css">
    <!-- <link rel="stylesheet" href="style.css"> -->
    <!-- <script src="code.js"></script> -->
    <title>webbyformy</title>
</head>
<body id="mainContainer">
    <div class="left">
        <h1>This is a webform</h1>
        <div class="buttonRow">
            <button type="button" id="viewDB_Btn">View db-not used, test other pages</button>
            <button type="button" id="startEmbroidery_Btn">Start</button>
            <button type="button" id="calibrate_Btn">Stop-not implemented</button>
        </div>

        <div class="message">
            <h1 id="talkToUser"> </h1>
        </div>
    </div>

    <div class="right">
        <h3>Records</h3>
        <div class="tableWrapper">
            <table id="dataBaseTable">
                <thead>
                    <tr>
                        <th>#</th>
                        <th>Name</th>
                        <th>Value</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>1</td>
                        <td>2</td>
                        <td>3</td>
                    </tr>
                    <tr>
                        <td>1</td>
                        <td>2</td>
                        <td>3</td>
                    </tr>
                    <!-- JS will add rows -->
                </tbody>
            </table>
        </div>
    </div>
</body>
</html>
</html>
)rawliteral";

#endif
