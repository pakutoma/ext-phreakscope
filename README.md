# ext-phreakscope

A PHP extension used by pakutoma/phreakscope.
It enables a low-overhead sampling profiler by accessing the Zend VM execution stack directly from a separate thread.

## Installation
As [PIE](https://github.com/php/pie) is the new extension manager for PHP, you can install this extension using it.
```bash
pie install pakutoma/ext-phreakscope
```

## Acknowledgements
This extension is heavily inspired by [nikic/sample_prof](https://github.com/nikic/sample_prof).