import { seriesStatistics } from './statistics';
import { MeasurementSeries } from './models';

const point = (time: number, mean: number, min: number, max: number, count: number) =>
  ({ time, mean, min, max, count, received_count: 0 });

const temperature: MeasurementSeries = { group: 'a', sensor: 's1', metric: 'temperature', unit: '°C', points: [
  point(0, 20, 19, 21, 1),
  point(10_000, 23, 22, 25, 3),
  point(20_000, 18, 17, 18.5, 2),
] };

describe('Series statistics', () => {
  it('combines buckets into exact min/max and a sample-weighted mean', () => {
    const [stats] = seriesStatistics([temperature], 10);
    expect(stats).toMatchObject({ group: 'a', sensor: 's1', metric: 'temperature', unit: '°C', count: 6, min: 17, max: 25 });
    // (20·1 + 23·3 + 18·2) / 6, not the unweighted (20 + 23 + 18) / 3
    expect(stats.mean).toBeCloseTo(125 / 6);
  });

  it('restricts the figures to buckets overlapping the zoom range', () => {
    const [stats] = seriesStatistics([temperature], 10, { from: 15_000, to: 30_000 });
    expect(stats).toMatchObject({ count: 5, min: 17, max: 25 });
    expect(stats.mean).toBeCloseTo((23 * 3 + 18 * 2) / 5);
  });

  it('omits series without samples in the period', () => {
    expect(seriesStatistics([temperature], 10, { from: 40_000, to: 50_000 })).toEqual([]);
    expect(seriesStatistics([{ ...temperature, points: [] }], 10)).toEqual([]);
  });
});
