loadColor();

var rate = 250;
var timerMax = 3;
var timer = -1;
var dark = false;
var hold = false;
var autoHold = false;
var autoHoldButton = document.getElementById("AutoHold");
var holdButton = document.getElementById("Hold");
var indicators = document.getElementById("ScaleIndicators");
var footer = document.getElementById("Footer");
var scaleWeight = document.getElementById("ScaleWeight");
var scaleWeightOunces = document.getElementById("ScaleWeightOunces");

setInterval(function() {
    var weight = 100.27;
    var ounces = 0;

    if (url != null && url != "" && !url.includes("ESPVAL"))
    {
        weight = Number(httpGet(url));
    }

    if (weight % 1 != 0)
    {
        var remainder = weight % 1;
        ounces = remainder * 16;
        ounces = Math.round(ounces);
    }

    if (autoHold)
    {
        if (weight > 0.2 && timer == -1)
        {
            timer = (1000 / rate) * timerMax;
        }

        if (timer >= 0)
        {
            timer--;
            indicators.innerHTML = "WEIGHING";
        }
    
        if (weight > 0.2 && timer <= 0)
        {
            timer = -1;
            hold = true;
        }
        
        autoHoldButton.style.color = "#00FF00";
    }
    else
    {
        autoHoldButton.style.color = "#AAAAAA";
    }

    var weightLbsOnly = Math.floor(weight);

    if (!hold)
    {
        scaleWeight.innerHTML = weightLbsOnly;
        scaleWeightOunces.innerHTML = ounces;
        holdButton.style.color = "#AAAAAA";
    }
    else
    {
        holdButton.style.color = "#FF0000";
        indicators.innerHTML = "HOLD";
    }
}, rate);

function manualHold()
{
    hold = !hold;
}

function toggleAutoHold()
{
    autoHold = !autoHold;
}

function goDark()
{
    dark = true;
    footer.hidden = true;
}

function colorSelected(color)
{
    console.log("Changed readout color to " + color);
    var readout = document.getElementById("Readout");
    readout.style.color = color;
    setCookie("ReadoutColor", color);
}

function loadColor()
{
    var readoutColor = getCookie("ReadoutColor");

    if (readoutColor == "")
    {
        readoutColor = "#00CCFF";
    }
    
    colorSelected(readoutColor);
}