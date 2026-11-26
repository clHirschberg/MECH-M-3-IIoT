import { MeasurementSeries } from './models';
import { ChartTimeRange } from './measurement-chart';

export interface SeriesStatistics {
  group: string;
  sensor: string;
  metric: MeasurementSeries['metric'];
  unit: string;
  count: number;
  min: number;
  max: number;
  mean: number;
}

/**
 * Aggregates the per-interval buckets of each series into figures for the whole period.
 * Min/max are exact (min of minima, max of maxima); the mean is weighted by the number of
 * samples per bucket, so buckets with few readings do not distort it.
 * With a zoom range, every bucket overlapping [from, to) counts, so edges are bucket-accurate.
 */
export function seriesStatistics(series: MeasurementSeries[], intervalSeconds: number,
  range: ChartTimeRange | null = null): SeriesStatistics[] {
  const width = intervalSeconds * 1000;
  return series.flatMap(s => {
    const points = s.points.filter(p => p.count > 0
      && (!range || (p.time + width > range.from && p.time < range.to)));
    const count = points.reduce((n, p) => n + p.count, 0);
    if (count === 0) return [];
    return [{
      group: s.group, sensor: s.sensor, metric: s.metric, unit: s.unit, count,
      min: Math.min(...points.map(p => p.min)),
      max: Math.max(...points.map(p => p.max)),
      mean: points.reduce((sum, p) => sum + p.mean * p.count, 0) / count,
    }];
  });
}
