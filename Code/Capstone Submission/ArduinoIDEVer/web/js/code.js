// Setup button events 
document.addEventListener("DOMContentLoaded", function() 
{
    LoadDB();
    document.getElementById("viewDB_Btn").addEventListener("click", ViewDB);
    document.getElementById("startEmbroidery_Btn").addEventListener("click", StartEmbroidery);
});
//-------------------------------------------------------------
// Function:   LoadDB()
// Purpose:    
//-------------------------------------------------------------
function LoadDB()
{
    console.log("Initial db loading");
    var uploadData = {};
    uploadData["action"] = "getDBData"; 
    // Make Fetch call to server
    FetchRequest("POST", uploadData, "json", LoadDBSuccess, SharedError);
}
//-------------------------------------------------------------
// Function:   LoadDBSuccess(returnedData)
// Purpose:    
//-------------------------------------------------------------
function LoadDBSuccess(returnedData)
{
    console.log(returnedData);
    console.log("DB success");
}
//-------------------------------------------------------------
// Function:   StartEmbroidery()
// Purpose:    
//-------------------------------------------------------------
function StartEmbroidery()
{
    console.log("Start button clicked");
    var uploadData = {};
    uploadData["action"] = "startEmbroidery"; // Action
    // Make Fetch call to server
    FetchRequest("POST", uploadData, "json", StartEmbroiderySuccess, SharedError);
}
//-------------------------------------------------------------
// Function:   StartEmbroiderySuccess(returnedData)
// Purpose:    
//-------------------------------------------------------------
function StartEmbroiderySuccess(returnedData)
{
    console.log(returnedData);
    console.log("Start success");
}
//-------------------------------------------------------------
// Function:   ViewDB()
// Purpose:    This function is fake news
//-------------------------------------------------------------
function ViewDB()
{
    console.log("View DB button clicked");
    window.location.href = "viewDB";
    // var uploadData = {};
    // uploadData["action"] = "viewDB"; // Action to tell your backend what to do
    // // Make Fetch call to server
    // FetchRequest("POST", uploadData, "text", ViewDBSuccess, SharedError);
}
//-------------------------------------------------------------
// Function:   ViewDBSuccess(returnedData)
// Purpose:    This function is also fake news
//-------------------------------------------------------------
function ViewDBSuccess(returnedData)
{
    console.log("View DB button success");
    // After the server responds, navigate to the new page
    window.location.href = "viewDB"; // loading other html page
}

//-------------------------------------------------------------
// Function:   SharedError(error)
// Purpose:    Blanket error check for all data send backs
//-------------------------------------------------------------
function SharedError(error) 
{
    console.error("Fetch error:", error.message || error);
}
// Basically ajax call function
function FetchRequest(requestType, data, responseType, successFunction, errorFunction)
{
    let request = 
    {
        // needs to be "method"
        method: requestType,
        headers: { "Content-Type": "application/json" }
    };
    // Only include if data exists
    if (data) 
    {
        request.body = JSON.stringify(data);
    }
    // Call fetch with action type (GET/POST), options
    fetch(data["action"], request)
    // Then go onto checking if an error occured
    .then(response => {
        if (!response.ok) throw new Error("Network response was not ok");
        // Set response expectation to either text or json
        return (responseType === "json") ? response.json() : response.text();
    })
    // result is the data we get from the prev .then
    .then(result => successFunction(result))
    .catch(error => errorFunction(error));
}
