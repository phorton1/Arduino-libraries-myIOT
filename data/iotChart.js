//------------------------------------------------
// iotChart.js
//------------------------------------------------
// Dashboard Elements
//
//     Will contain "chartN-type" which will group the dashboard elements
//     into a number of charts.  The types will be "binary", "voltage", etc,
//     and it should be possible to combine different types intelligently
//     into a single chart.
//
//     The initial implementation just supports a binary chart and
//     a single group, and in fact, the machinery for handling the
//     dashboard elements is not yet in place.
//
// Chart Updating
//
//     The javascript is responsible for updating the charts, initially
//     upon startup, and then (a) upon any changes to the elements within
//     the chart and/or (b) on an appropriate timer, depending on the scale,
//     for "animation".
//
// Chart Basics
//
//     The user can select the number of hours they wish to "view" for each,
//     chart. The defaults should be stored in browser memory, but for now
//     the default will be whatever works for testing.
//
//     The minimum in-memory chart is 1 hour, though they can be zoomed
//     in from there (tbd).
//
//          Hour
//          6 hours
//          12 hours
//          1 day = 24 hours
//          5 days = 120 hours
//          week = 148 hours
//          30 days = 720 hours
//
// Data Set
//
//     chart: {
//          now: NNNN,      // current server time utc seconds since 1970
//          series: [
//              ELEMENT_ID1 : [
//                    [time, value],
//                    [time, value]   ],
//              ELEMENT_ID2 : [
//                    [time, value],
//                    [time, value]   ],
//              ],
//          }
//
//      It should be possible to incrementally update the data set to minimize
//      network traffic, although a redraw in javascript is still cpu and memory
//      intensive.
//
// Chart Updating
//
// Chart X-Axis Labels
//

var the_chart;
var chart_end;          // "now" in utc seconds since 1970
var chart_start;
var the_data;


var initial_chart_data =
{
    series: [
    [
        {x: 1, y: 1},
        {x: 2, y: 0},
        {x: 200, y: 1},
        {x: 205, y: 0},
        {x: 300, y: 1},
        {x: 305, y: 1}
    ],
    [
        {x: 101, y: 1},
        {x: 102, y: 0},
        {x: 250, y: 1},
        {x: 255, y: 0},
        {x: 350, y: 1},
        {x: 365, y: 1}
    ]]
};

var initial_chart_options = {
  // width: 500,
  height: 100,
  axisX: {
    type: Chartist.FixedScaleAxis,
    // type: Chartist.AutoScaleAxis,
    onlyInteger: true },
  axisY: {
    type: Chartist.FixedScaleAxis,
    ticks: [0, 1],
    low: 0,
    high: 1 },
    lineSmooth: Chartist.Interpolation.step(),
    showPoint: false,
};


function setChartBounds()
{
    var chart_hours = $('#chart_hours').val();
    chart_end = Math.round(Date.now() / 1000);
    chart_start = chart_end - (chart_hours * 60 * 60);
}



function localTimeString(utc_secs)
    // javascript automatically applies local timezone to dates
{
    var dt = new Date(utc_secs * 1000);
    return dt;
}



function updateChart(obj)
{
    setChartBounds();
    initial_chart_options.axisX["low"] = chart_start;
    initial_chart_options.axisX["high"] = chart_end;
    the_chart.update(obj,initial_chart_options);
}


function updateChartData()
{
    var chart_hours = $('#chart_hours').val();
    var since = Math.round(Date.now() / 1000) - chart_hours * 60 * 60;
    sendCommand("get_chart_data",{"since":since});
}


function initChart()
{
    setChartBounds();
    initial_chart_options.axisX["low"] = chart_start;
    initial_chart_options.axisX["high"] = chart_end;
    // Create a new line chart object where as first parameter we pass in a selector
    // that is resolving to our chart container element. The Second parameter
    // is the actual data object.
    the_chart = new Chartist.Line('.ct-chart', initial_chart_data, initial_chart_options);
    // setInterval(updateChartData,2000);
}