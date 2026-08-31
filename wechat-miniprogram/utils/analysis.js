function linearRegression(points, resolutionKPa) {
  var rows = (points || []).filter(function (point) {
    return point.valid !== false && isFinite(Number(point.externalValue)) && isFinite(Number(point.pressureKPa))
  })
  var n = rows.length
  if (n < 2) {
    return {
      ready: false,
      message: "至少需要 2 个有效数据点",
      n: n,
      equation: "数据不足",
      residuals: []
    }
  }
  var sumX = 0
  var sumY = 0
  var sumXX = 0
  var sumXY = 0
  rows.forEach(function (point) {
    var x = Number(point.externalValue)
    var y = Number(point.pressureKPa)
    sumX += x
    sumY += y
    sumXX += x * x
    sumXY += x * y
  })
  var denominator = n * sumXX - sumX * sumX
  var slope = Math.abs(denominator) < 1e-12 ? 0 : (n * sumXY - sumX * sumY) / denominator
  var intercept = (sumY - slope * sumX) / n
  var meanY = sumY / n
  var ssRes = 0
  var ssTot = 0
  var residuals = rows.map(function (point) {
    var predicted = slope * Number(point.externalValue) + intercept
    var residual = Number(point.pressureKPa) - predicted
    ssRes += residual * residual
    ssTot += Math.pow(Number(point.pressureKPa) - meanY, 2)
    return {
      id: point.id,
      index: point.index,
      x: Number(point.externalValue),
      actual: Number(point.pressureKPa),
      predicted: predicted,
      residual: residual,
      outlier: false
    }
  })
  var rmse = Math.sqrt(ssRes / Math.max(1, n - 2))
  residuals.forEach(function (row) {
    row.outlier = n >= 4 && rmse > 0 && Math.abs(row.residual) > 3 * rmse
  })
  var r2 = ssTot < 1e-12 ? 1 : Math.max(0, 1 - ssRes / ssTot)
  var r = (slope < 0 ? -1 : 1) * Math.sqrt(r2)
  var typeA = Math.sqrt(ssRes / Math.max(1, n - 1)) / Math.sqrt(n)
  var typeB = (Number(resolutionKPa) || .1) / Math.sqrt(12)
  var combined = Math.sqrt(typeA * typeA + typeB * typeB)
  var sign = intercept >= 0 ? " + " : " − "
  return {
    ready: n >= 3,
    n: n,
    slope: slope,
    intercept: intercept,
    equation: "P = " + slope.toFixed(4) + "x" + sign + Math.abs(intercept).toFixed(4),
    r: r,
    r2: r2,
    rmse: rmse,
    typeA: typeA,
    typeB: typeB,
    combined: combined,
    expanded: combined * 2,
    outlierCount: residuals.filter(function (row) { return row.outlier }).length,
    residuals: residuals
  }
}

module.exports = {linearRegression: linearRegression}
